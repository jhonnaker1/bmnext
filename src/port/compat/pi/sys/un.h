/*
 * sys/un.h - for SLiRP (src/slirp/), on the Pi: newlib for Circle has none.
 * SLiRP's code for Unix-domain sockets (its X and terminal redirections,
 * which BMNeXT doesn't use) needs the type to build; bmnet's socket()
 * refuses AF_UNIX.
 *
 * This file is part of BMNeXT, a bare metal NeXT emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#ifndef BMNEXT_SYS_UN_H_
#define BMNEXT_SYS_UN_H_

#include <sys/socket.h>

#ifndef AF_UNIX
#define AF_UNIX 1
#endif

struct sockaddr_un {
	sa_family_t sun_family;
	char sun_path[108];
};

#endif
