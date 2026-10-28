/*
 * rpc_sockets.c - the sockets of Previous's NFS, NetInfo and other
 * servers, on one thread, for BMNeXT.
 *
 * Replaces Previous's src/slirp/rpc/csocket.c, udpsocket.c and
 * tcpsocket.c (same structures, same functions). Previous's servers
 * (portmap, mount, NFS, NetInfo, bootparam, DNS) listen on the host's
 * loopback address, and SLiRP passes them the NeXT's requests; there each
 * socket has a thread waiting in recv() or accept(). BMNeXT has one
 * thread: here nothing waits. rpc_sockets_poll(), called with SLiRP's own
 * polling (enet_next.c), takes what has arrived on each open socket and
 * accepts new connections, and the servers' code runs as before.
 *
 * A TCP request (a record: a 4-byte length, then the data) can arrive in
 * pieces, an NFS write split over several of the NeXT's packets; SLiRP,
 * which sends the pieces, runs on this same thread, so waiting for the
 * rest would wait for ever. What has arrived is kept, and the request
 * goes to its server once it is all there.
 *
 * This file is part of BMNeXT, a bare metal NeXT emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#include <slirp.h>
#include <stdlib.h>

#include "rpc/csocket.h"
#include "rpc/udpsocket.h"
#include "rpc/tcpsocket.h"

#include "bmnext.h"

/* ---- The open sockets ------------------------------------------------- */

#define MAX_OPEN 96
#define MAX_LISTEN 16

/* A socket being polled, and how much of a TCP record has arrived. */
static struct {
	struct csocket_t *cs;
	uint32_t have;
} open_sockets[MAX_OPEN];

static struct tcpsocket_t *listening[MAX_LISTEN];

static void watch(struct csocket_t *cs)
{
	int i;
	for (i = 0; i < MAX_OPEN; i++) {
		if (open_sockets[i].cs == NULL) {
			open_sockets[i].cs = cs;
			open_sockets[i].have = 0;
			return;
		}
	}
	printf("BMNeXT: too many RPC connections\n");
}

static void unwatch(struct csocket_t *cs)
{
	int i;
	for (i = 0; i < MAX_OPEN; i++)
		if (open_sockets[i].cs == cs)
			open_sockets[i].cs = NULL;
}

/* ---- csocket.c ------------------------------------------------------- */

void sock_close(sock_t socket)
{
	shutdown(socket, SHUT_RDWR);
	close(socket);
}

struct csocket_t *csocket_init(int nType, int serverPort, void *server)
{
	struct csocket_t *cs = malloc(sizeof *cs);
	if (cs == NULL)
		return NULL;
	cs->m_nType = nType;
	cs->m_Socket = INVALID_SOCKET;
	cs->m_pListener = NULL;
	cs->m_nActive = 0;
	cs->m_hThread = NULL;
	cs->m_serverPort = serverPort;
	cs->m_pServer = server;
	memset(&cs->m_RemoteAddr, 0, sizeof(cs->m_RemoteAddr));
	cs->m_Input = xdr_init();
	cs->m_Output = xdr_init();
	return cs;
}

struct csocket_t *csocket_uninit(struct csocket_t *cs)
{
	if (cs == NULL)
		return NULL;
	csocket_close(cs);
	xdr_uninit(cs->m_Input);
	xdr_uninit(cs->m_Output);
	free(cs);
	return NULL;
}

void csocket_open(struct csocket_t *cs, sock_t socket, socket_listener_t *pListener,
                  struct sockaddr_in *pRemoteAddr)
{
	csocket_close(cs);
	cs->m_Socket = socket;
	cs->m_pListener = pListener;
	if (pRemoteAddr != NULL)
		cs->m_RemoteAddr = *pRemoteAddr;
	if (cs->m_Socket != INVALID_SOCKET) {
		cs->m_nActive = 1;
		watch(cs);
	}
}

void csocket_close(struct csocket_t *cs)
{
	unwatch(cs);
	if (cs->m_Socket != INVALID_SOCKET) {
		sock_close(cs->m_Socket);
		cs->m_Socket = INVALID_SOCKET;
	}
	cs->m_nActive = 0;
}

void csocket_send(struct csocket_t *cs)
{
	ssize_t nBytes = 0;
	if (cs->m_Socket == INVALID_SOCKET)
		return;
	if (cs->m_nType == SOCK_STREAM) {
		xdr_write_long_at(cs->m_Output->head, 0x80000000 | cs->m_Output->size);
		cs->m_Output->size += 4;
		nBytes = send(cs->m_Socket, (const char *) cs->m_Output->head, cs->m_Output->size, 0);
	} else if (cs->m_nType == SOCK_DGRAM) {
		nBytes = sendto(cs->m_Socket, (const char *) cs->m_Output->head, cs->m_Output->size, 0,
		                (struct sockaddr *) &cs->m_RemoteAddr, sizeof(struct sockaddr));
	}
	if (nBytes < 0)
		perror("[RPC] Socket send");
	else if (nBytes != cs->m_Output->size)
		perror("[RPC] Socket send, size mismatch");
}

/* The request in cs->m_Input (NBYTES of it) to the server. */
static void deliver(struct csocket_t *cs, uint32_t nBytes)
{
	cs->m_Input->data = cs->m_Input->head;
	cs->m_Output->data = cs->m_Output->head;
	if (cs->m_nType == SOCK_STREAM) {
		cs->m_Input->data += 4;     /* past the record's header */
		cs->m_Output->data += 4;    /* room for the answer's */
		nBytes -= 4;
	}
	cs->m_Input->size = nBytes;
	cs->m_Output->size = 0;
	if (cs->m_pListener != NULL)
		cs->m_pListener(cs);
}

/* The other end has gone (or an error): the socket is done with, as when
   Previous's thread returns. */
static void ended(int slot)
{
	struct csocket_t *cs = open_sockets[slot].cs;
	open_sockets[slot].cs = NULL;
	cs->m_nActive = 0;
	if (cs->m_Socket != INVALID_SOCKET) {
		sock_close(cs->m_Socket);
		cs->m_Socket = INVALID_SOCKET;
	}
}

/* What has arrived on one socket. */
static void poll_socket(int slot)
{
	struct csocket_t *cs = open_sockets[slot].cs;
	uint32_t cap = cs->m_Input->capacity;
	ssize_t n;
	int rounds;

	if (cs->m_nType == SOCK_DGRAM) {
		/* Each datagram is a request. */
		for (rounds = 0; rounds < 16 && open_sockets[slot].cs == cs; rounds++) {
			socklen_t size = sizeof(cs->m_RemoteAddr);
			n = recvfrom(cs->m_Socket, (void *) cs->m_Input->head, cap, MSG_DONTWAIT,
			             (struct sockaddr *) &cs->m_RemoteAddr, &size);
			if (n > 0)
				deliver(cs, (uint32_t) n);
			else
				break;
		}
		return;
	}

	for (rounds = 0; rounds < 16 && open_sockets[slot].cs == cs; rounds++) {
		uint32_t have = open_sockets[slot].have;
		uint32_t want;
		if (have >= cap) {
			printf("BMNeXT: RPC request too big (%u bytes)\n", (unsigned) have);
			ended(slot);
			return;
		}
		n = recv(cs->m_Socket, (void *) (cs->m_Input->head + have), cap - have, MSG_DONTWAIT);
		if (n == 0) {
			ended(slot);           /* closed by the other end */
			return;
		}
		if (n < 0) {
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
				if (errno != EBADF)
					perror("[RPC] Socket receive");
				ended(slot);
			}
			return;
		}
		have += (uint32_t) n;
		open_sockets[slot].have = have;
		if (have < 4)
			continue;
		want = (~0x80000000u & xdr_read_long_at(cs->m_Input->head)) + 4;
		if (have < want)
			continue;              /* more to come */
		open_sockets[slot].have = 0;
		if (have > want) {
			/* The next record has begun: Previous's own code doesn't
			   keep it either (a client waits for each answer). */
			printf("BMNeXT: RPC record followed by %u more bytes, dropped\n",
			       (unsigned) (have - want));
		}
		deliver(cs, want);
	}
}

/* Previous's thread body: not used (rpc_sockets_poll() does it). */
void csocket_run(struct csocket_t *cs)
{
}

/* ---- udpsocket.c -------------------------------------------------------- */

struct udpsocket_t *udpsocket_init(socket_listener_t *pListener, void *pServer)
{
	struct udpsocket_t *us = malloc(sizeof *us);
	if (us) {
		us->m_nPort = 0;
		us->m_Server = pServer;
		us->m_Socket = 0;
		us->m_pSocket = NULL;
		us->m_nClosed = 1;
		us->m_pListener = pListener;
	}
	return us;
}

struct udpsocket_t *udpsocket_uninit(struct udpsocket_t *us)
{
	udpsocket_close(us);
	free(us);
	return NULL;
}

uint16_t udpsocket_open(struct udpsocket_t *us, uint16_t nPort)
{
	struct sockaddr_in localAddr;
	socklen_t size = 64 * 1024, len = sizeof(size);

	udpsocket_close(us);
	us->m_Socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (us->m_Socket == INVALID_SOCKET)
		return 0;
	setsockopt(us->m_Socket, SOL_SOCKET, SO_SNDBUF, &size, len);
	setsockopt(us->m_Socket, SOL_SOCKET, SO_RCVBUF, &size, len);
	memset(&localAddr, 0, sizeof(localAddr));
	localAddr.sin_family = AF_INET;
	localAddr.sin_port = 0;
	localAddr.sin_addr = loopback_addr;
	if (bind(us->m_Socket, (struct sockaddr *) &localAddr, sizeof(struct sockaddr)) < 0) {
		sock_close(us->m_Socket);
		return 0;
	}
	size = sizeof(localAddr);
	if (getsockname(us->m_Socket, (struct sockaddr *) &localAddr, &size) < 0) {
		sock_close(us->m_Socket);
		return 0;
	}
	us->m_nPort = nPort ? nPort : ntohs(localAddr.sin_port);
	us->m_nClosed = 0;
	us->m_pSocket = csocket_init(SOCK_DGRAM, us->m_nPort, us->m_Server);
	if (us->m_pSocket) {
		csocket_open(us->m_pSocket, us->m_Socket, us->m_pListener, NULL);
		return ntohs(localAddr.sin_port);
	}
	return 0;
}

void udpsocket_close(struct udpsocket_t *us)
{
	if (us->m_nClosed)
		return;
	us->m_nClosed = 1;
	us->m_pSocket = csocket_uninit(us->m_pSocket);
}

void udpspcket_run(struct udpsocket_t *us)
{
}

/* ---- tcpsocket.c -------------------------------------------------------- */

#define BACKLOG 16

struct tcpsocket_t *tcpsocket_init(socket_listener_t *pListener, void *pServer)
{
	struct tcpsocket_t *ts = malloc(sizeof *ts);
	if (ts) {
		ts->m_nPort = 0;
		ts->m_Server = pServer;
		ts->m_ServerSocket = 0;
		ts->m_pSockets = NULL;
		ts->m_nClosed = 1;
		ts->m_pListener = pListener;
		ts->m_hThread = NULL;
	}
	return ts;
}

struct tcpsocket_t *tcpsocket_uninit(struct tcpsocket_t *ts)
{
	tcpsocket_close(ts);
	free(ts);
	return NULL;
}

uint16_t tcpsocket_open(struct tcpsocket_t *ts, uint16_t nPort)
{
	struct sockaddr_in localAddr;
	socklen_t size;
	int i;

	tcpsocket_close(ts);
	ts->m_ServerSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (ts->m_ServerSocket == INVALID_SOCKET)
		return 0;
	memset(&localAddr, 0, sizeof(localAddr));
	localAddr.sin_family = AF_INET;
	localAddr.sin_port = 0;
	localAddr.sin_addr = loopback_addr;
	if (bind(ts->m_ServerSocket, (struct sockaddr *) &localAddr, sizeof(localAddr)) < 0) {
		sock_close(ts->m_ServerSocket);
		return 0;
	}
	size = sizeof(localAddr);
	if (getsockname(ts->m_ServerSocket, (struct sockaddr *) &localAddr, &size) < 0) {
		sock_close(ts->m_ServerSocket);
		return 0;
	}
	ts->m_nPort = nPort ? nPort : ntohs(localAddr.sin_port);
	if (listen(ts->m_ServerSocket, BACKLOG) < 0) {
		sock_close(ts->m_ServerSocket);
		return 0;
	}
	ts->m_pSockets = malloc(sizeof(struct csocket_t *[BACKLOG]));
	for (i = 0; i < BACKLOG; i++)
		ts->m_pSockets[i] = csocket_init(SOCK_STREAM, ts->m_nPort, ts->m_Server);
	ts->m_nClosed = 0;
	for (i = 0; i < MAX_LISTEN; i++) {
		if (listening[i] == NULL) {
			listening[i] = ts;
			break;
		}
	}
	return ntohs(localAddr.sin_port);
}

void tcpsocket_close(struct tcpsocket_t *ts)
{
	int i;
	if (ts->m_nClosed)
		return;
	ts->m_nClosed = 1;
	for (i = 0; i < MAX_LISTEN; i++)
		if (listening[i] == ts)
			listening[i] = NULL;
	sock_close(ts->m_ServerSocket);
	if (ts->m_pSockets != NULL) {
		for (i = 0; i < BACKLOG; i++)
			ts->m_pSockets[i] = csocket_uninit(ts->m_pSockets[i]);
		free(ts->m_pSockets);
		ts->m_pSockets = NULL;
	}
}

/* A connection waiting on a listening socket: to an idle csocket. */
static void accept_waiting(struct tcpsocket_t *ts)
{
	fd_set rfds;
	struct timeval tv = {0, 0};
	struct sockaddr_in remoteAddr;
	socklen_t nSize = sizeof(remoteAddr);
	sock_t s;
	int i;

	FD_ZERO(&rfds);
	FD_SET(ts->m_ServerSocket, &rfds);
	if (select(ts->m_ServerSocket + 1, &rfds, NULL, NULL, &tv) <= 0)
		return;
	s = accept(ts->m_ServerSocket, (struct sockaddr *) &remoteAddr, &nSize);
	if (s == INVALID_SOCKET)
		return;
	for (i = 0; i < BACKLOG; i++) {
		if (!ts->m_pSockets[i]->m_nActive) {
			csocket_open(ts->m_pSockets[i], s, ts->m_pListener, &remoteAddr);
			return;
		}
	}
	printf("BMNeXT: no room for another RPC connection\n");
	sock_close(s);
}

void tcpsocket_run(struct tcpsocket_t *ts)
{
}

/* ---- Polling -------------------------------------------------------------- */

void rpc_sockets_poll(void)
{
	int i;
	for (i = 0; i < MAX_LISTEN; i++)
		if (listening[i] != NULL && !listening[i]->m_nClosed)
			accept_waiting(listening[i]);
	for (i = 0; i < MAX_OPEN; i++)
		if (open_sockets[i].cs != NULL)
			poll_socket(i);
}
