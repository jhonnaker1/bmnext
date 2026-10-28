/*
 * bmnet.c - the sockets of the NeXT's network, for BMNeXT.
 *
 * SLiRP (src/slirp/) turns the NeXT's connections into the host's, and
 * Previous's own servers (src/slirp/rpc/) listen on the host's 127.0.0.1,
 * where SLiRP passes them the NeXT's requests. Both use BSD sockets; here
 * they are BMNeXT's (compat/bmnet.h says why, and how the calls get here):
 *
 *   127.0.0.1   in memory: UDP datagrams go straight to the socket bound to
 *               their port; a TCP connect() to a listening socket makes a
 *               pair of sockets, each with a buffer the other writes into
 *   the rest    the kernel's TCP and UDP (host.h): UDP sends and receives
 *               without waiting; a TCP connect, which can take seconds,
 *               runs in the background (host_run_in_background()), and the
 *               socket turns writable when it is done, as SLiRP's
 *               non-blocking connect() expects
 *
 * Nothing here waits: select() reports what is ready now. All on the
 * emulator's thread (enet_next.c, rpc_sockets.c), but for the background
 * connects, which only fill in their own job.
 *
 * Nothing comes in from outside: a listening socket bound to all addresses
 * (SLiRP's redirections of the host's ports to the NeXT's) is only ever
 * reached from 127.0.0.1.
 *
 * This file is part of BMNeXT, a bare metal NeXT emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#define BMNET_IMPLEMENTATION
#include "compat/bmnet.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "../host.h"

#define RING_SIZE   (64 * 1024)   /* a 127.0.0.1 TCP socket's buffer */
#define MAX_QUEUED  32            /* datagrams waiting on a UDP socket */
#define MAX_PENDING 16            /* connections waiting to be accepted */
#define XBUF_SIZE   2048          /* read ahead from the kernel's TCP */

enum {
	S_FREE,
	S_NEW,         /* made, not connected or listening */
	S_LISTEN,
	S_CONNECTING,  /* the kernel's connect in the background */
	S_CONNECTED,
	S_FAILED,      /* the connect failed (err) */
	S_CLOSED,      /* the other end has gone */
	S_ORPHAN       /* closed while connecting: free once the connect ends */
};

struct dgram {
	struct dgram *next;
	uint32_t ip;          /* where from, network byte order */
	uint16_t port;
	size_t len;
	uint8_t data[];
};

struct connect_job {
	volatile int done;
	int result;           /* host_tcp_connect()'s */
	char host[16], port[8];
};

struct bsock {
	int state;
	int type;             /* SOCK_STREAM or SOCK_DGRAM */
	int err;              /* for SO_ERROR */
	uint32_t lip, rip;    /* network byte order */
	uint16_t lport, rport;
	/* 127.0.0.1 */
	int lo;
	int peer;             /* the other end's index, -1 */
	int peer_gone;        /* the other end has closed (or shut down) */
	uint8_t *ring;
	size_t ring_head, ring_len;
	struct dgram *dq_head, *dq_tail;
	int dq_count;
	int pending[MAX_PENDING];
	int npending;
	/* the kernel's */
	int tcp, udp;
	struct connect_job *job;
	uint8_t *xbuf;
	size_t xpos, xlen;
	struct dgram *xdg;
};

static struct bsock socks[BMNET_MAX];
static uint16_t next_port = 49152;

/* ---- Helpers --------------------------------------------------------------- */

static struct bsock *get(int fd)
{
	int i = bmnet_fd_index(fd);
	if (i < 0 || socks[i].state == S_FREE || socks[i].state == S_ORPHAN)
		return NULL;
	return &socks[i];
}

static int fd_of(struct bsock *s)
{
	return (int) (s - socks) + BMNET_FD_BASE;
}

static int fail(int e)
{
	errno = e;
	return -1;
}

static int is_loopback(uint32_t ip)
{
	return (ntohl(ip) & 0xff000000u) == 0x7f000000u;
}

static int port_in_use(int type, uint16_t port)
{
	int i;
	for (i = 0; i < BMNET_MAX; i++)
		if (socks[i].state != S_FREE && socks[i].type == type && socks[i].lport == port)
			return 1;
	return 0;
}

static uint16_t ephemeral_port(int type)
{
	int tries;
	for (tries = 0; tries < 16384; tries++) {
		uint16_t p = htons(next_port);
		next_port = next_port >= 65535 ? 49152 : next_port + 1;
		if (!port_in_use(type, p))
			return p;
	}
	return 0;
}

static void fill_addr(struct sockaddr *addr, socklen_t *len, uint32_t ip, uint16_t port)
{
	struct sockaddr_in in;
	if (addr == NULL || len == NULL)
		return;
	memset(&in, 0, sizeof in);
	in.sin_family = AF_INET;
	in.sin_addr.s_addr = ip;
	in.sin_port = port;
	if (*len > sizeof in)
		*len = sizeof in;
	memcpy(addr, &in, *len);
	*len = sizeof in;
}

static int read_addr(const struct sockaddr *addr, socklen_t len, uint32_t *ip, uint16_t *port)
{
	const struct sockaddr_in *in = (const struct sockaddr_in *) addr;
	if (addr == NULL || len < sizeof(struct sockaddr_in) || in->sin_family != AF_INET)
		return -1;
	*ip = in->sin_addr.s_addr;
	*port = in->sin_port;
	return 0;
}

static void free_queue(struct dgram **head)
{
	while (*head) {
		struct dgram *d = *head;
		*head = d->next;
		free(d);
	}
}

static void release(struct bsock *s)
{
	free(s->ring);
	free(s->xbuf);
	free(s->xdg);
	free_queue(&s->dq_head);
	memset(s, 0, sizeof *s);
	s->state = S_FREE;
	s->peer = -1;
	s->tcp = s->udp = -1;
}

/* ---- The ring of a 127.0.0.1 TCP socket ------------------------------------ */

static size_t ring_put(struct bsock *s, const uint8_t *data, size_t len)
{
	size_t n = 0;
	if (s->ring == NULL) {
		s->ring = malloc(RING_SIZE);
		if (s->ring == NULL)
			return 0;
	}
	while (n < len && s->ring_len < RING_SIZE) {
		size_t tail = (s->ring_head + s->ring_len) % RING_SIZE;
		size_t chunk = RING_SIZE - tail;
		if (chunk > RING_SIZE - s->ring_len)
			chunk = RING_SIZE - s->ring_len;
		if (chunk > len - n)
			chunk = len - n;
		memcpy(s->ring + tail, data + n, chunk);
		s->ring_len += chunk;
		n += chunk;
	}
	return n;
}

static size_t ring_get(struct bsock *s, uint8_t *data, size_t len)
{
	size_t n = 0;
	while (n < len && s->ring_len > 0) {
		size_t chunk = RING_SIZE - s->ring_head;
		if (chunk > s->ring_len)
			chunk = s->ring_len;
		if (chunk > len - n)
			chunk = len - n;
		memcpy(data + n, s->ring + s->ring_head, chunk);
		s->ring_head = (s->ring_head + chunk) % RING_SIZE;
		s->ring_len -= chunk;
		n += chunk;
	}
	return n;
}

/* ---- Background connects ------------------------------------------------- */

static void connect_job_run(void *arg)
{
	struct connect_job *job = arg;
	job->result = host_tcp_connect(job->host, job->port);
	__sync_synchronize();
	job->done = 1;
}

/* Connects that have finished: their sockets connected or failed; a socket
   closed meanwhile is freed now. */
static void reap_jobs(void)
{
	int i;
	for (i = 0; i < BMNET_MAX; i++) {
		struct bsock *s = &socks[i];
		if (s->job == NULL || !s->job->done)
			continue;
		__sync_synchronize();
		if (s->state == S_ORPHAN) {
			if (s->job->result >= 0)
				host_tcp_close(s->job->result);
			free(s->job);
			s->job = NULL;
			release(s);
			continue;
		}
		if (s->job->result >= 0) {
			s->tcp = s->job->result;
			s->state = S_CONNECTED;
		} else {
			s->state = S_FAILED;
			s->err = s->job->result == -2 ? EHOSTUNREACH : ECONNREFUSED;
		}
		free(s->job);
		s->job = NULL;
	}
}

/* ---- What has arrived from the kernel ------------------------------------- */

/* A TCP socket's read ahead: >0 bytes waiting, 0 none, -1 closed. */
static int tcp_fill(struct bsock *s)
{
	int got;
	if (s->xpos < s->xlen)
		return 1;
	if (s->tcp < 0)
		return -1;
	if (s->xbuf == NULL) {
		s->xbuf = malloc(XBUF_SIZE);
		if (s->xbuf == NULL)
			return 0;
	}
	got = host_tcp_recv(s->tcp, s->xbuf, XBUF_SIZE);
	if (got < 0) {
		host_tcp_close(s->tcp);
		s->tcp = -1;
		s->state = S_CLOSED;
		return -1;
	}
	s->xpos = 0;
	s->xlen = (size_t) got;
	return got > 0;
}

/* A UDP socket's datagram from outside, kept until read. */
static int udp_fill(struct bsock *s)
{
	uint8_t buf[2048];
	uint32_t ip;
	unsigned port;
	int got;
	if (s->xdg != NULL)
		return 1;
	if (s->udp < 0)
		return 0;
	got = host_udp_recv(s->udp, buf, sizeof buf, &ip, &port);
	if (got <= 0)
		return 0;
	s->xdg = malloc(sizeof(struct dgram) + (size_t) got);
	if (s->xdg == NULL)
		return 0;
	s->xdg->next = NULL;
	s->xdg->ip = ip;
	s->xdg->port = htons((uint16_t) port);
	s->xdg->len = (size_t) got;
	memcpy(s->xdg->data, buf, (size_t) got);
	return 1;
}

/* ---- Sockets ------------------------------------------------------------- */

int bmnet_socket(int domain, int type, int protocol)
{
	int i;
	if (domain != AF_INET || (type != SOCK_STREAM && type != SOCK_DGRAM))
		return fail(EAFNOSUPPORT);   /* no raw (ICMP) sockets: no ping */
	for (i = 0; i < BMNET_MAX; i++) {
		if (socks[i].state == S_FREE) {
			release(&socks[i]);
			socks[i].state = S_NEW;
			socks[i].type = type;
			return i + BMNET_FD_BASE;
		}
	}
	return fail(EMFILE);
}

int bmnet_bind(int fd, const struct sockaddr *addr, socklen_t len)
{
	struct bsock *s = get(fd);
	uint32_t ip;
	uint16_t port;
	if (s == NULL)
		return fail(EBADF);
	if (read_addr(addr, len, &ip, &port) < 0)
		return fail(EINVAL);
	if (port == 0)
		port = ephemeral_port(s->type);
	else if (port_in_use(s->type, port))
		return fail(EADDRINUSE);
	s->lip = ip;
	s->lport = port;
	s->lo = is_loopback(ip);
	return 0;
}

int bmnet_listen(int fd, int backlog)
{
	struct bsock *s = get(fd);
	if (s == NULL)
		return fail(EBADF);
	if (s->type != SOCK_STREAM)
		return fail(EOPNOTSUPP);
	if (s->lport == 0)
		s->lport = ephemeral_port(SOCK_STREAM);
	s->state = S_LISTEN;
	return 0;
}

int bmnet_accept(int fd, struct sockaddr *addr, socklen_t *len)
{
	struct bsock *s = get(fd), *c;
	int i;
	if (s == NULL)
		return fail(EBADF);
	if (s->state != S_LISTEN)
		return fail(EINVAL);
	if (s->npending == 0)
		return fail(EAGAIN);
	i = s->pending[0];
	memmove(s->pending, s->pending + 1, (size_t) (s->npending - 1) * sizeof s->pending[0]);
	s->npending--;
	c = &socks[i];
	fill_addr(addr, len, c->rip, c->rport);
	return fd_of(c);
}

/* A TCP connect to 127.0.0.1: to a listening socket's waiting list. */
static int connect_loopback(struct bsock *s, uint32_t ip, uint16_t port)
{
	int i, j;
	for (i = 0; i < BMNET_MAX; i++) {
		struct bsock *l = &socks[i];
		if (l->state != S_LISTEN || l->lport != port || !(l->lo || l->lip == INADDR_ANY))
			continue;
		if (l->npending >= MAX_PENDING)
			break;
		for (j = 0; j < BMNET_MAX; j++)
			if (socks[j].state == S_FREE)
				break;
		if (j == BMNET_MAX)
			return fail(EMFILE);
		release(&socks[j]);
		if (s->lport == 0)
			s->lport = ephemeral_port(SOCK_STREAM);
		s->lip = htonl(INADDR_LOOPBACK);
		s->lo = 1;
		s->rip = ip;
		s->rport = port;
		s->state = S_CONNECTED;
		s->peer = j;
		socks[j].state = S_CONNECTED;
		socks[j].type = SOCK_STREAM;
		socks[j].lo = 1;
		socks[j].lip = htonl(INADDR_LOOPBACK);
		socks[j].lport = port;
		socks[j].rip = s->lip;
		socks[j].rport = s->lport;
		socks[j].peer = (int) (s - socks);
		l->pending[l->npending++] = j;
		return 0;
	}
	s->state = S_FAILED;
	s->err = ECONNREFUSED;
	return fail(ECONNREFUSED);
}

int bmnet_connect(int fd, const struct sockaddr *addr, socklen_t len)
{
	struct bsock *s = get(fd);
	uint32_t ip;
	uint16_t port;
	struct connect_job *job;
	struct in_addr a;
	if (s == NULL)
		return fail(EBADF);
	if (read_addr(addr, len, &ip, &port) < 0)
		return fail(EINVAL);
	if (s->type == SOCK_DGRAM) {
		/* A default destination for send(). */
		s->rip = ip;
		s->rport = port;
		return 0;
	}
	if (s->state != S_NEW)
		return fail(s->state == S_CONNECTING ? EALREADY : EISCONN);
	if (is_loopback(ip))
		return connect_loopback(s, ip, port);
	job = calloc(1, sizeof *job);
	if (job == NULL)
		return fail(ENOMEM);
	a.s_addr = ip;
	snprintf(job->host, sizeof job->host, "%s", inet_ntoa(a));
	snprintf(job->port, sizeof job->port, "%u", (unsigned) ntohs(port));
	s->rip = ip;
	s->rport = port;
	s->job = job;
	s->state = S_CONNECTING;
	if (!host_run_in_background(connect_job_run, job)) {
		free(job);
		s->job = NULL;
		s->state = S_FAILED;
		s->err = ENETUNREACH;
		return fail(ENETUNREACH);
	}
	return fail(EINPROGRESS);
}

/* ---- Sending and receiving ------------------------------------------------- */

static ssize_t stream_send(struct bsock *s, const void *buf, size_t len)
{
	int sent;
	switch (s->state) {
	case S_CONNECTING:
		return fail(EAGAIN);
	case S_FAILED: {
		int e = s->err ? s->err : ECONNREFUSED;
		return fail(e);
	}
	case S_CONNECTED:
		break;
	default:
		return fail(s->state == S_CLOSED ? EPIPE : ENOTCONN);
	}
	if (len == 0)
		return 0;
	if (s->lo) {
		size_t n;
		if (s->peer < 0 || s->peer_gone)
			return fail(EPIPE);
		n = ring_put(&socks[s->peer], buf, len);
		return n > 0 ? (ssize_t) n : fail(EAGAIN);
	}
	sent = host_tcp_send(s->tcp, buf, (unsigned) len);
	if (sent < 0) {
		s->state = S_CLOSED;
		return fail(EPIPE);
	}
	return sent > 0 ? sent : fail(EAGAIN);
}

static ssize_t stream_recv(struct bsock *s, void *buf, size_t len)
{
	size_t n;
	int r;
	if (s->state == S_CONNECTING)
		return fail(EAGAIN);
	if (s->state == S_FAILED)
		return fail(s->err ? s->err : ECONNREFUSED);
	if (len == 0)
		return 0;
	if (s->lo) {
		n = ring_get(s, buf, len);
		if (n > 0)
			return (ssize_t) n;
		return s->peer_gone || s->peer < 0 ? 0 : fail(EAGAIN);
	}
	r = tcp_fill(s);
	if (r < 0)
		return 0;    /* closed: end of file */
	if (r == 0)
		return fail(EAGAIN);
	n = s->xlen - s->xpos;
	if (n > len)
		n = len;
	memcpy(buf, s->xbuf + s->xpos, n);
	s->xpos += n;
	return (ssize_t) n;
}

/* A datagram to 127.0.0.1:PORT: to the socket bound to it. */
static ssize_t loopback_sendto(struct bsock *s, const void *buf, size_t len, uint16_t port)
{
	int i;
	if (s->lport == 0)
		s->lport = ephemeral_port(SOCK_DGRAM);
	for (i = 0; i < BMNET_MAX; i++) {
		struct bsock *d = &socks[i];
		struct dgram *g;
		if (d->state == S_FREE || d->state == S_ORPHAN || d->type != SOCK_DGRAM
		    || d->lport != port || !(d->lo || d->lip == INADDR_ANY))
			continue;
		if (d->dq_count >= MAX_QUEUED)
			return (ssize_t) len;   /* dropped, as a full network would */
		g = malloc(sizeof *g + len);
		if (g == NULL)
			return fail(ENOMEM);
		g->next = NULL;
		g->ip = htonl(INADDR_LOOPBACK);
		g->port = s->lport;
		g->len = len;
		memcpy(g->data, buf, len);
		if (d->dq_tail)
			d->dq_tail->next = g;
		else
			d->dq_head = g;
		d->dq_tail = g;
		d->dq_count++;
		return (ssize_t) len;
	}
	return (ssize_t) len;              /* nobody there: lost, as UDP is */
}

ssize_t bmnet_sendto(int fd, const void *buf, size_t len, int flags,
                     const struct sockaddr *to, socklen_t tolen)
{
	struct bsock *s = get(fd);
	uint32_t ip;
	uint16_t port;
	int sent;
	if (s == NULL)
		return bmnet_write(fd, buf, len);
	if (s->type == SOCK_STREAM)
		return stream_send(s, buf, len);
	if (to == NULL) {
		ip = s->rip;
		port = s->rport;
	} else if (read_addr(to, tolen, &ip, &port) < 0) {
		return fail(EINVAL);
	}
	if (port == 0)
		return fail(EDESTADDRREQ);
	if (is_loopback(ip))
		return loopback_sendto(s, buf, len, port);
	if (s->udp < 0) {
		s->udp = host_udp_open();
		if (s->udp < 0)
			return fail(ENETUNREACH);
	}
	sent = host_udp_send(s->udp, ip, ntohs(port), buf, (unsigned) len);
	if (sent < 0)
		return fail(ENETUNREACH);
	return sent > 0 ? sent : (ssize_t) len;   /* a full queue drops it */
}

ssize_t bmnet_recvfrom(int fd, void *buf, size_t len, int flags,
                       struct sockaddr *from, socklen_t *fromlen)
{
	struct bsock *s = get(fd);
	struct dgram *g;
	size_t n;
	if (s == NULL)
		return bmnet_read(fd, buf, len);
	if (s->type == SOCK_STREAM) {
		ssize_t r = stream_recv(s, buf, len);
		if (r >= 0)
			fill_addr(from, fromlen, s->rip, s->rport);
		return r;
	}
	if (s->dq_head != NULL) {
		g = s->dq_head;
		s->dq_head = g->next;
		if (s->dq_head == NULL)
			s->dq_tail = NULL;
		s->dq_count--;
	} else if (udp_fill(s)) {
		g = s->xdg;
		s->xdg = NULL;
	} else {
		return fail(EAGAIN);
	}
	n = g->len < len ? g->len : len;
	memcpy(buf, g->data, n);
	fill_addr(from, fromlen, g->ip, g->port);
	free(g);
	return (ssize_t) n;
}

ssize_t bmnet_send(int fd, const void *buf, size_t len, int flags)
{
	return bmnet_sendto(fd, buf, len, flags, NULL, 0);
}

ssize_t bmnet_recv(int fd, void *buf, size_t len, int flags)
{
	return bmnet_recvfrom(fd, buf, len, flags, NULL, NULL);
}

ssize_t bmnet_read(int fd, void *buf, size_t len)
{
	if (bmnet_fd_index(fd) < 0)
		return read(fd, buf, len);
	return bmnet_recvfrom(fd, buf, len, 0, NULL, NULL);
}

ssize_t bmnet_write(int fd, const void *buf, size_t len)
{
	struct bsock *s;
	if (bmnet_fd_index(fd) < 0)
		return write(fd, buf, len);
	s = get(fd);
	if (s == NULL)
		return fail(EBADF);
	if (s->type == SOCK_STREAM)
		return stream_send(s, buf, len);
	return bmnet_sendto(fd, buf, len, 0, NULL, 0);
}

ssize_t bmnet_readv(int fd, const struct iovec *iov, int n)
{
	ssize_t total = 0;
	int i;
	for (i = 0; i < n; i++) {
		ssize_t r;
		if (iov[i].iov_len == 0)
			continue;
		r = bmnet_read(fd, iov[i].iov_base, iov[i].iov_len);
		if (r < 0)
			return total > 0 ? total : r;
		total += r;
		if ((size_t) r < iov[i].iov_len)
			break;
	}
	return total;
}

ssize_t bmnet_writev(int fd, const struct iovec *iov, int n)
{
	ssize_t total = 0;
	int i;
	for (i = 0; i < n; i++) {
		ssize_t r;
		if (iov[i].iov_len == 0)
			continue;
		r = bmnet_write(fd, iov[i].iov_base, iov[i].iov_len);
		if (r < 0)
			return total > 0 ? total : r;
		total += r;
		if ((size_t) r < iov[i].iov_len)
			break;
	}
	return total;
}

/* ---- Closing ---------------------------------------------------------------- */

int bmnet_shutdown(int fd, int how)
{
	struct bsock *s = get(fd);
	if (s == NULL)
		return fail(EBADF);
	if (s->lo && s->peer >= 0 && (how == SHUT_WR || how == SHUT_RDWR))
		socks[s->peer].peer_gone = 1;   /* the other end reads to its end */
	return 0;
}

int bmnet_close(int fd)
{
	struct bsock *s;
	int i;
	if (bmnet_fd_index(fd) < 0)
		return close(fd);
	s = get(fd);
	if (s == NULL)
		return fail(EBADF);
	if (s->lo && s->peer >= 0) {
		socks[s->peer].peer_gone = 1;
		socks[s->peer].peer = -1;
	}
	if (s->state == S_LISTEN) {
		/* Connections nobody accepted: their other ends see the end. */
		for (i = 0; i < s->npending; i++) {
			struct bsock *c = &socks[s->pending[i]];
			if (c->peer >= 0) {
				socks[c->peer].peer_gone = 1;
				socks[c->peer].peer = -1;
			}
			release(c);
		}
	}
	if (s->tcp >= 0)
		host_tcp_close(s->tcp);
	if (s->udp >= 0)
		host_udp_close(s->udp);
	if (s->state == S_CONNECTING && s->job != NULL) {
		/* The connect goes on in the background: freed when it ends. */
		s->state = S_ORPHAN;
		s->tcp = s->udp = -1;
		return 0;
	}
	release(s);
	return 0;
}

/* ---- Waiting (not) ---------------------------------------------------------- */

static int readable(struct bsock *s)
{
	switch (s->state) {
	case S_LISTEN:
		return s->npending > 0;
	case S_FAILED:
	case S_CLOSED:
		return 1;
	case S_CONNECTING:
		return 0;
	default:
		break;
	}
	if (s->type == SOCK_DGRAM)
		return s->dq_count > 0 || udp_fill(s);
	if (s->state != S_CONNECTED)
		return 0;
	if (s->lo)
		return s->ring_len > 0 || s->peer_gone || s->peer < 0;
	return tcp_fill(s) != 0;
}

static int writable(struct bsock *s)
{
	if (s->type == SOCK_DGRAM)
		return 1;
	if (s->state == S_FAILED)
		return 1;
	if (s->state != S_CONNECTED)
		return 0;
	if (s->lo)
		return s->peer >= 0 && !s->peer_gone && socks[s->peer].ring_len < RING_SIZE;
	return 1;
}

int bmnet_select(int nfds, bmnet_fd_set *r, bmnet_fd_set *w, bmnet_fd_set *x,
                 struct timeval *timeout)
{
	bmnet_fd_set rr, ww;
	int fd, count = 0;
	reap_jobs();
	memset(&rr, 0, sizeof rr);
	memset(&ww, 0, sizeof ww);
	for (fd = BMNET_FD_BASE; fd < nfds && fd < BMNET_FD_BASE + BMNET_MAX; fd++) {
		struct bsock *s = get(fd);
		if (s == NULL)
			continue;
		if (r && bmnet_fd_get(fd, r) && readable(s)) {
			bmnet_fd_put(fd, &rr, 1);
			count++;
		}
		if (w && bmnet_fd_get(fd, w) && writable(s)) {
			bmnet_fd_put(fd, &ww, 1);
			count++;
		}
	}
	if (r)
		*r = rr;
	if (w)
		*w = ww;
	if (x)
		memset(x, 0, sizeof *x);
	return count;
}

/* ---- Options ---------------------------------------------------------------- */

int bmnet_setsockopt(int fd, int level, int name, const void *value, socklen_t len)
{
	return get(fd) ? 0 : fail(EBADF);
}

int bmnet_getsockopt(int fd, int level, int name, void *value, socklen_t *len)
{
	struct bsock *s = get(fd);
	if (s == NULL)
		return fail(EBADF);
	if (level == SOL_SOCKET && name == SO_ERROR && value && len && *len >= sizeof(int)) {
		reap_jobs();
		*(int *) value = s->state == S_FAILED ? s->err : 0;
		*len = sizeof(int);
		return 0;
	}
	if (value && len && *len >= sizeof(int)) {
		*(int *) value = 0;
		*len = sizeof(int);
	}
	return 0;
}

int bmnet_getsockname(int fd, struct sockaddr *addr, socklen_t *len)
{
	struct bsock *s = get(fd);
	if (s == NULL)
		return fail(EBADF);
	fill_addr(addr, len, s->lip, s->lport);
	return 0;
}

int bmnet_getpeername(int fd, struct sockaddr *addr, socklen_t *len)
{
	struct bsock *s = get(fd);
	if (s == NULL)
		return fail(EBADF);
	if (s->state != S_CONNECTED)
		return fail(ENOTCONN);
	fill_addr(addr, len, s->rip, s->rport);
	return 0;
}

int bmnet_ioctl(int fd, unsigned long request, ...)
{
	struct bsock *s;
	va_list ap;
	void *arg;
	va_start(ap, request);
	arg = va_arg(ap, void *);
	va_end(ap);
	if (bmnet_fd_index(fd) < 0)
		return fail(ENOTTY);      /* nothing here asks it of a file */
	s = get(fd);
	if (s == NULL)
		return fail(EBADF);
	if (request == FIONBIO)
		return 0;                 /* bmnet's sockets never wait anyway */
	if (request == FIONREAD && arg != NULL) {
		size_t n = 0;
		if (s->type == SOCK_DGRAM) {
			if (s->dq_head)
				n = s->dq_head->len;
			else if (udp_fill(s))
				n = s->xdg->len;
		} else if (s->lo) {
			n = s->ring_len;
		} else if (s->state == S_CONNECTED && tcp_fill(s) > 0) {
			n = s->xlen - s->xpos;
		}
		*(int *) arg = (int) n;
		return 0;
	}
	return fail(EINVAL);
}

int bmnet_fcntl(int fd, int cmd, ...)
{
	va_list ap;
	int arg;
	va_start(ap, cmd);
	arg = va_arg(ap, int);
	va_end(ap);
	if (bmnet_fd_index(fd) < 0)
		return fcntl(fd, cmd, arg);
	if (get(fd) == NULL)
		return fail(EBADF);
	if (cmd == F_GETFL)
		return O_RDWR | O_NONBLOCK;
	return 0;                     /* F_SETFL O_NONBLOCK: never waits anyway */
}

int bmnet_gethostname(char *name, size_t len)
{
	snprintf(name, len, "bmnext");
	return 0;
}

/* Names aren't looked up here: SLiRP asks for its own address this way,
   and does without it (127.0.0.1). */
int bmnet_getaddrinfo(const char *node, const char *service,
                      const struct addrinfo *hints, struct addrinfo **res)
{
	return EAI_FAIL;
}
