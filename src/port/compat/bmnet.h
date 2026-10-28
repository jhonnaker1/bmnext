/*
 * bmnet.h - the sockets of the NeXT's network, BMNeXT's (src/port/bmnet.c).
 *
 * Included before every file of SLiRP (src/slirp/) and Previous's servers
 * (src/slirp/rpc/), and enet_next.c and rpc_sockets.c (-include, see the
 * Makefiles): their socket calls (socket(), connect(), select(), and the
 * read(), write() and close() they also use on sockets) become bmnet's.
 * bmnet's sockets have descriptors of their own (BMNET_FD_BASE and up);
 * read(), write(), close(), fcntl() and ioctl() on any other descriptor
 * (the NFS server's files on the SD card) go to the C library as before.
 *
 * Why not the C library's sockets: on the Pi they are circle-newlib's,
 * which can't talk to the host itself (Previous's servers listen on
 * 127.0.0.1), wait in connect() (SLiRP must not), and hold the lock on
 * the table of open files while they wait (see src/host.h). bmnet keeps
 * 127.0.0.1 in memory, and goes to the kernel's TCP and UDP (host.h) for
 * the rest; the PC test harness runs the same code, with its host.h on
 * the PC's own sockets.
 *
 * The system's headers are included first, so that their declarations
 * keep their names; the macros below only change the calls made after.
 *
 * This file is part of BMNeXT, a bare metal NeXT emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#ifndef BMNEXT_BMNET_H_
#define BMNEXT_BMNET_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef BMNEXT_HOST_LIBC
#include <sys/ioctl.h>
#include <sys/uio.h>
#else
/* What newlib for Circle doesn't have (compat/pi/sys/uio.h and ioctl.h
   bring this header in). */
#ifndef BMNET_IOVEC_DEFINED
#define BMNET_IOVEC_DEFINED
struct iovec {
	void *iov_base;
	size_t iov_len;
};
#endif
#ifndef FIONBIO
#define FIONBIO  0x8004667e
#endif
#ifndef FIONREAD
#define FIONREAD 0x4004667f
#endif
#ifndef SHUT_RD
#define SHUT_RD   0
#define SHUT_WR   1
#define SHUT_RDWR 2
#endif
#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0x40
#endif
#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK ((in_addr_t) 0x7f000001)
#endif
/* inet_aton() is SLiRP's own (src/slirp/misc.c). */
int inet_aton(const char *cp, struct in_addr *addr);
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* bmnet's descriptors, and a set of them for select(). */
#define BMNET_FD_BASE 1024
#define BMNET_MAX     256

typedef struct {
	uint32_t bits[BMNET_MAX / 32];
} bmnet_fd_set;

static inline int bmnet_fd_index(int fd)
{
	return fd >= BMNET_FD_BASE && fd < BMNET_FD_BASE + BMNET_MAX ? fd - BMNET_FD_BASE : -1;
}

static inline void bmnet_fd_put(int fd, bmnet_fd_set *set, int on)
{
	int i = bmnet_fd_index(fd);
	if (set == NULL || i < 0)
		return;
	if (on)
		set->bits[i / 32] |= 1u << (i % 32);
	else
		set->bits[i / 32] &= ~(1u << (i % 32));
}

static inline int bmnet_fd_get(int fd, const bmnet_fd_set *set)
{
	int i = bmnet_fd_index(fd);
	return set != NULL && i >= 0 && (set->bits[i / 32] >> (i % 32)) & 1;
}

int bmnet_socket(int domain, int type, int protocol);
int bmnet_bind(int fd, const struct sockaddr *addr, socklen_t len);
int bmnet_listen(int fd, int backlog);
int bmnet_accept(int fd, struct sockaddr *addr, socklen_t *len);
int bmnet_connect(int fd, const struct sockaddr *addr, socklen_t len);
ssize_t bmnet_send(int fd, const void *buf, size_t len, int flags);
ssize_t bmnet_recv(int fd, void *buf, size_t len, int flags);
ssize_t bmnet_sendto(int fd, const void *buf, size_t len, int flags,
                     const struct sockaddr *to, socklen_t tolen);
ssize_t bmnet_recvfrom(int fd, void *buf, size_t len, int flags,
                       struct sockaddr *from, socklen_t *fromlen);
int bmnet_shutdown(int fd, int how);
int bmnet_setsockopt(int fd, int level, int name, const void *value, socklen_t len);
int bmnet_getsockopt(int fd, int level, int name, void *value, socklen_t *len);
int bmnet_getsockname(int fd, struct sockaddr *addr, socklen_t *len);
int bmnet_getpeername(int fd, struct sockaddr *addr, socklen_t *len);
int bmnet_select(int nfds, bmnet_fd_set *r, bmnet_fd_set *w, bmnet_fd_set *x,
                 struct timeval *timeout);
int bmnet_close(int fd);
ssize_t bmnet_read(int fd, void *buf, size_t len);
ssize_t bmnet_write(int fd, const void *buf, size_t len);
ssize_t bmnet_readv(int fd, const struct iovec *iov, int n);
ssize_t bmnet_writev(int fd, const struct iovec *iov, int n);
int bmnet_ioctl(int fd, unsigned long request, ...);
int bmnet_fcntl(int fd, int cmd, ...);
int bmnet_gethostname(char *name, size_t len);
int bmnet_getaddrinfo(const char *node, const char *service,
                      const struct addrinfo *hints, struct addrinfo **res);

#ifdef __cplusplus
}
#endif

#ifndef BMNET_IMPLEMENTATION

#define fd_set bmnet_fd_set
#undef FD_ZERO
#undef FD_SET
#undef FD_CLR
#undef FD_ISSET
#define FD_ZERO(set)      memset((set), 0, sizeof(bmnet_fd_set))
#define FD_SET(fd, set)   bmnet_fd_put((fd), (set), 1)
#define FD_CLR(fd, set)   bmnet_fd_put((fd), (set), 0)
#define FD_ISSET(fd, set) bmnet_fd_get((fd), (set))

#define socket(d, t, p)            bmnet_socket(d, t, p)
#define bind(f, a, l)              bmnet_bind(f, a, l)
#define listen(f, b)               bmnet_listen(f, b)
#define accept(f, a, l)            bmnet_accept(f, a, l)
#define connect(f, a, l)           bmnet_connect(f, a, l)
#define send(f, b, l, fl)          bmnet_send(f, b, l, fl)
#define recv(f, b, l, fl)          bmnet_recv(f, b, l, fl)
#define sendto(f, b, l, fl, t, tl) bmnet_sendto(f, b, l, fl, t, tl)
#define recvfrom(f, b, l, fl, s, sl) bmnet_recvfrom(f, b, l, fl, s, sl)
#define shutdown(f, h)             bmnet_shutdown(f, h)
#define setsockopt(f, lv, n, v, l) bmnet_setsockopt(f, lv, n, v, l)
#define getsockopt(f, lv, n, v, l) bmnet_getsockopt(f, lv, n, v, l)
#define getsockname(f, a, l)       bmnet_getsockname(f, a, l)
#define getpeername(f, a, l)       bmnet_getpeername(f, a, l)
#define select(n, r, w, x, t)      bmnet_select(n, r, w, x, t)
#define close(f)                   bmnet_close(f)
#define read(f, b, l)              bmnet_read(f, b, l)
#define write(f, b, l)             bmnet_write(f, b, l)
#define readv(f, v, n)             bmnet_readv(f, v, n)
#define writev(f, v, n)            bmnet_writev(f, v, n)
#define ioctl(...)                 bmnet_ioctl(__VA_ARGS__)
#define fcntl(...)                 bmnet_fcntl(__VA_ARGS__)
#define gethostname(n, l)          bmnet_gethostname(n, l)
#define getaddrinfo(n, s, h, r)    bmnet_getaddrinfo(n, s, h, r)

#endif /* BMNET_IMPLEMENTATION */

#endif /* BMNEXT_BMNET_H_ */
