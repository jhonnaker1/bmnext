/*
 * bmnet_test.c - tests src/port/bmnet.c, the NeXT network's sockets.
 *
 *   make -C tools/hostsim build/bmnet_test && tools/hostsim/build/bmnet_test
 *
 * bmnet's 127.0.0.1 (in memory), and its way out through the kernel's TCP
 * and UDP (src/host.h). Here those are test versions on the PC's own
 * sockets, where 192.0.2.1 (an address kept for examples) stands for a
 * server "out there", and is really the PC's 127.0.0.1: so a connection
 * bmnet makes out goes through host_tcp_connect() in the background, as on
 * the Pi, to a server this test runs itself. Also: read(), write() and
 * close() of an ordinary file still reach the C library.
 *
 * This file is part of BMNeXT, a bare metal NeXT emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "../../src/host.h"
/* bmnet's functions, without its macros: this file's own socket calls
   (the test's servers, and host.h's test versions) are the PC's. */
#define BMNET_IMPLEMENTATION
#include "../../src/port/compat/bmnet.h"

static int failures;

#define CHECK(cond, what) do { \
	if (cond) printf("PASS  bmnet: %s\n", what); \
	else { printf("FAIL  bmnet: %s (line %d)\n", what, __LINE__); failures++; } \
} while (0)

/* ---- host.h, on the PC's sockets; 192.0.2.1 is the PC's 127.0.0.1 --------- */

#define OUTSIDE "192.0.2.1"

static uint32_t real_ip(uint32_t ip)
{
	return ip == inet_addr(OUTSIDE) ? htonl(INADDR_LOOPBACK) : ip;
}

int host_tcp_connect(const char *host, const char *port)
{
	struct sockaddr_in to;
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	memset(&to, 0, sizeof to);
	to.sin_family = AF_INET;
	to.sin_port = htons((uint16_t) atoi(port));
	to.sin_addr.s_addr = real_ip(inet_addr(host));
	if (fd >= 0 && connect(fd, (struct sockaddr *) &to, sizeof to) < 0) {
		close(fd);
		fd = -1;
	}
	return fd;
}

int host_tcp_recv(int c, void *buffer, unsigned size)
{
	ssize_t got = recv(c, buffer, size, MSG_DONTWAIT);
	if (got > 0)
		return (int) got;
	return got < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) ? 0 : -1;
}

int host_tcp_send(int c, const void *buffer, unsigned size)
{
	ssize_t sent = send(c, buffer, size, MSG_DONTWAIT);
	if (sent >= 0)
		return (int) sent;
	return errno == EAGAIN || errno == EWOULDBLOCK ? 0 : -1;
}

void host_tcp_close(int c)
{
	close(c);
}

int host_udp_open(void)
{
	return socket(AF_INET, SOCK_DGRAM, 0);
}

int host_udp_send(int h, uint32_t ip, unsigned port, const void *data, unsigned size)
{
	struct sockaddr_in to;
	memset(&to, 0, sizeof to);
	to.sin_family = AF_INET;
	to.sin_port = htons((uint16_t) port);
	to.sin_addr.s_addr = real_ip(ip);
	return (int) sendto(h, data, size, 0, (struct sockaddr *) &to, sizeof to);
}

int host_udp_recv(int h, void *buffer, unsigned size, uint32_t *ip, unsigned *port)
{
	struct sockaddr_in from;
	socklen_t len = sizeof from;
	ssize_t got = recvfrom(h, buffer, size, MSG_DONTWAIT, (struct sockaddr *) &from, &len);
	if (got < 0)
		return errno == EAGAIN || errno == EWOULDBLOCK ? 0 : -1;
	*ip = from.sin_addr.s_addr == htonl(INADDR_LOOPBACK) ? inet_addr(OUTSIDE) : from.sin_addr.s_addr;
	*port = ntohs(from.sin_port);
	return (int) got;
}

void host_udp_close(int h)
{
	close(h);
}

struct job {
	void (*fn)(void *);
	void *arg;
};

static void *job_thread(void *p)
{
	struct job j = *(struct job *) p;
	free(p);
	j.fn(j.arg);
	return NULL;
}

int host_run_in_background(void (*fn)(void *arg), void *arg)
{
	pthread_t t;
	struct job *j = malloc(sizeof *j);
	j->fn = fn;
	j->arg = arg;
	if (pthread_create(&t, NULL, job_thread, j) != 0)
		return 0;
	pthread_detach(t);
	return 1;
}

/* ---- Helpers ------------------------------------------------------------- */

static struct sockaddr_in addr(const char *ip, uint16_t port)
{
	struct sockaddr_in a;
	memset(&a, 0, sizeof a);
	a.sin_family = AF_INET;
	a.sin_addr.s_addr = inet_addr(ip);
	a.sin_port = htons(port);
	return a;
}

static uint16_t bound_port(int fd)
{
	struct sockaddr_in a;
	socklen_t len = sizeof a;
	bmnet_getsockname(fd, (struct sockaddr *) &a, &len);
	return ntohs(a.sin_port);
}

static int ready(int fd, int for_write, int tries)
{
	while (tries-- > 0) {
		bmnet_fd_set set;
		memset(&set, 0, sizeof set);
		bmnet_fd_put(fd, &set, 1);
		if (bmnet_select(fd + 1, for_write ? NULL : &set, for_write ? &set : NULL, NULL, NULL) > 0)
			return 1;
		usleep(1000);
	}
	return 0;
}

/* ---- The tests ------------------------------------------------------------ */

static void loopback_udp(void)
{
	int a = bmnet_socket(AF_INET, SOCK_DGRAM, 0), b = bmnet_socket(AF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in la = addr("127.0.0.1", 0), lb = addr("127.0.0.1", 0), from, to;
	socklen_t flen = sizeof from;
	char buf[32];
	ssize_t n;
	bmnet_bind(a, (struct sockaddr *) &la, sizeof la);
	bmnet_bind(b, (struct sockaddr *) &lb, sizeof lb);
	CHECK(a >= BMNET_FD_BASE && bound_port(a) != 0 && bound_port(a) != bound_port(b),
	      "UDP sockets bound to 127.0.0.1 get their own ports");
	to = addr("127.0.0.1", bound_port(b));
	bmnet_sendto(a, "ping", 4, 0, (struct sockaddr *) &to, sizeof to);
	CHECK(ready(b, 0, 1), "a datagram to 127.0.0.1 is there at once");
	n = bmnet_recvfrom(b, buf, sizeof buf, 0, (struct sockaddr *) &from, &flen);
	CHECK(n == 4 && memcmp(buf, "ping", 4) == 0 && ntohs(from.sin_port) == bound_port(a),
	      "it comes from the sender's port");
	n = bmnet_recvfrom(b, buf, sizeof buf, 0, NULL, NULL);
	CHECK(n < 0 && errno == EAGAIN, "no more: EAGAIN, never waiting");
	bmnet_close(a);
	bmnet_close(b);
}

static void loopback_tcp(void)
{
	int l = bmnet_socket(AF_INET, SOCK_STREAM, 0), c = bmnet_socket(AF_INET, SOCK_STREAM, 0), s;
	struct sockaddr_in la = addr("127.0.0.1", 0), to;
	char buf[64];
	ssize_t n;
	bmnet_bind(l, (struct sockaddr *) &la, sizeof la);
	bmnet_listen(l, 4);
	to = addr("127.0.0.1", bound_port(l));
	CHECK(bmnet_connect(c, (struct sockaddr *) &to, sizeof to) == 0,
	      "a TCP connect to a listening 127.0.0.1 socket succeeds");
	CHECK(ready(l, 0, 1), "the listening socket has it waiting");
	s = bmnet_accept(l, NULL, NULL);
	CHECK(s >= BMNET_FD_BASE, "accepted");
	bmnet_send(c, "hello", 5, 0);
	n = bmnet_recv(s, buf, sizeof buf, 0);
	CHECK(n == 5 && memcmp(buf, "hello", 5) == 0, "what one end sends, the other reads");
	bmnet_write(s, "world", 5);
	n = bmnet_read(c, buf, sizeof buf);
	CHECK(n == 5 && memcmp(buf, "world", 5) == 0, "and the other way (read, write)");
	bmnet_close(c);
	CHECK(ready(s, 0, 1) && bmnet_recv(s, buf, sizeof buf, 0) == 0,
	      "closing one end is the end of file at the other");
	to = addr("127.0.0.1", 9);
	c = bmnet_socket(AF_INET, SOCK_STREAM, 0);
	CHECK(bmnet_connect(c, (struct sockaddr *) &to, sizeof to) < 0 && errno == ECONNREFUSED,
	      "nobody listening: refused");
	bmnet_close(c);
	bmnet_close(s);
	bmnet_close(l);
}

/* A server "out there" (the PC's 127.0.0.1, as 192.0.2.1) that answers
   what it gets, upper case. */
static int server_fd;

static void *tcp_server(void *unused)
{
	char buf[64];
	int c = accept(server_fd, NULL, NULL);
	ssize_t n = recv(c, buf, sizeof buf, 0), i;
	for (i = 0; i < n; i++)
		if (buf[i] >= 'a' && buf[i] <= 'z')
			buf[i] -= 32;
	send(c, buf, (size_t) n, 0);
	close(c);
	return NULL;
}

static void outside_tcp(void)
{
	struct sockaddr_in a = addr("127.0.0.1", 0), to;
	socklen_t len = sizeof a;
	pthread_t t;
	char buf[64];
	ssize_t n = -1;
	int c, i, r;
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	bind(server_fd, (struct sockaddr *) &a, sizeof a);
	listen(server_fd, 1);
	getsockname(server_fd, (struct sockaddr *) &a, &len);
	pthread_create(&t, NULL, tcp_server, NULL);

	c = bmnet_socket(AF_INET, SOCK_STREAM, 0);
	to = addr(OUTSIDE, ntohs(a.sin_port));
	r = bmnet_connect(c, (struct sockaddr *) &to, sizeof to);
	CHECK(r < 0 && errno == EINPROGRESS, "a TCP connect out goes on in the background");
	CHECK(ready(c, 1, 3000), "the socket turns writable when connected");
	CHECK(bmnet_send(c, "", 0, 0) == 0, "and a send of nothing then succeeds (SLiRP's test)");
	bmnet_send(c, "next", 4, 0);
	for (i = 0; i < 3000 && n <= 0; i++) {
		n = bmnet_recv(c, buf, sizeof buf, 0);
		if (n < 0)
			usleep(1000);
	}
	CHECK(n == 4 && memcmp(buf, "NEXT", 4) == 0, "data goes out and comes back");
	for (i = 0, n = -1; i < 3000 && n < 0; i++) {
		n = bmnet_recv(c, buf, sizeof buf, 0);
		if (n < 0)
			usleep(1000);
	}
	CHECK(n == 0, "the server closing is the end of file");
	bmnet_close(c);
	pthread_join(t, NULL);
	close(server_fd);

	c = bmnet_socket(AF_INET, SOCK_STREAM, 0);
	to = addr(OUTSIDE, 9);
	bmnet_connect(c, (struct sockaddr *) &to, sizeof to);
	CHECK(ready(c, 1, 3000), "a refused connect turns writable too");
	CHECK(bmnet_send(c, "", 0, 0) < 0, "and its send fails (SLiRP gives up)");
	bmnet_close(c);
}

static void outside_udp(void)
{
	struct sockaddr_in a = addr("127.0.0.1", 0), from, to;
	socklen_t len = sizeof a, flen = sizeof from;
	char buf[64];
	ssize_t n = -1;
	int echo = socket(AF_INET, SOCK_DGRAM, 0), u, i;
	bind(echo, (struct sockaddr *) &a, sizeof a);
	getsockname(echo, (struct sockaddr *) &a, &len);

	u = bmnet_socket(AF_INET, SOCK_DGRAM, 0);
	to = addr(OUTSIDE, ntohs(a.sin_port));
	bmnet_sendto(u, "dns?", 4, 0, (struct sockaddr *) &to, sizeof to);
	n = recvfrom(echo, buf, sizeof buf, 0, (struct sockaddr *) &from, &flen);
	CHECK(n == 4 && memcmp(buf, "dns?", 4) == 0, "a datagram out reaches its server");
	sendto(echo, "dns!", 4, 0, (struct sockaddr *) &from, flen);
	for (i = 0, n = -1; i < 3000 && n < 0; i++) {
		n = bmnet_recvfrom(u, buf, sizeof buf, 0, (struct sockaddr *) &from, &flen);
		if (n < 0)
			usleep(1000);
	}
	CHECK(n == 4 && memcmp(buf, "dns!", 4) == 0 && from.sin_addr.s_addr == inet_addr(OUTSIDE),
	      "and its answer comes back, from the server");
	bmnet_close(u);
	close(echo);
}

static void files(void)
{
	char path[] = "/tmp/bmnet_test_XXXXXX", buf[8];
	int fd = mkstemp(path);
	CHECK(fd >= 0 && bmnet_write(fd, "file", 4) == 4, "write() on a file still writes it");
	lseek(fd, 0, SEEK_SET);
	CHECK(bmnet_read(fd, buf, sizeof buf) == 4 && memcmp(buf, "file", 4) == 0,
	      "read() on a file still reads it");
	CHECK(bmnet_close(fd) == 0, "close() on a file still closes it");
	unlink(path);
}

int main(void)
{
	loopback_udp();
	loopback_tcp();
	outside_tcp();
	outside_udp();
	files();
	if (failures)
		printf("%d bmnet test(s) failed.\n", failures);
	return failures != 0;
}
