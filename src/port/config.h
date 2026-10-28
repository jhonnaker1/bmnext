/*
 * config.h - Previous's build options for BMNeXT.
 *
 * Previous's CMake build normally writes this file (from
 * cmake/config-cmake.h). BMNeXT builds Previous with its own makefile
 * (src/port/previous.mk), so the options are set here by hand: no host
 * libraries but zlib (third_party/zlib), no libpng, libpcap or readline.
 *
 * The same options are used on the Pi (circle-newlib) and in the PC test
 * harness (tools/hostsim), so that both build the same code. Only what
 * newlib has is turned on.
 *
 * ENABLE_RENDERING_THREAD is set: in that mode Previous runs its CPU on the
 * main thread and calls the front end from it 200 times a second
 * (Main_EventHandler(), main_next.c), and the screen is repainted by the
 * front end rather than by a thread of Previous's (screen.c). Circle runs
 * the emulator on one core with no threads.
 *
 * This file is part of BMNeXT, a bare metal NeXT emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#ifndef BMNEXT_PREVIOUS_CONFIG_H_
#define BMNEXT_PREVIOUS_CONFIG_H_

/* zlib (third_party/zlib): gzipped disk images. */
#define HAVE_LIBZ 1
#define HAVE_ZLIB_H 1

/* newlib's headers and functions */
#define HAVE_STRINGS_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_GETTIMEOFDAY 1
#define HAVE_SETENV 1
#define HAVE_STRDUP 1
#define HAVE_FSEEKO 1
#define HAVE_FTELLO 1
#define HAVE_DIRENT_D_TYPE 1

/* The NeXT's memory is allocated 64K aligned (cpu/maccess.h) where the C
   library can. On the Pi it can't: newlib's aligned_alloc() is Circle's
   memalign(), which asserts the alignment is at most a cache line (64
   bytes), a panic that stops every core before anything is shown. There
   it is plain malloc() (Previous's own fall back), which Previous's memory
   banks don't mind. */
#ifdef BMNEXT_HOST_LIBC
#define HAVE_POSIX_MEMALIGN 1
#endif

/* struct stat's times: newlib's and Linux's are st_atim, st_mtim (POSIX),
   macOS's (the PC test harness on a Mac) st_atimespec, st_mtimespec. The
   NFS server (src/slirp/rpc/) reads them. */
#ifdef __APPLE__
#define HAVE_STRUCT_STAT_ST_ATIMESPEC 1
#define HAVE_STRUCT_STAT_ST_MTIMESPEC 1
#endif

/* newlib has neither, nor the d_reclen Previous's own scandir.c needs: on
   the Pi they are BMNeXT's (posix_extra.c); the PC's C library has them. */
#define HAVE_SCANDIR 1
#define HAVE_ALPHASORT 1

/* Where Previous would look for a system wide previous.cfg. BMNeXT
   passes its settings itself (src/port/main_next.c). */
#define CONFDIR "/"
#define BIN2DATADIR "."

/* The NeXT's DSP56001. */
#define ENABLE_DSP_EMU 1

/* The CPU runs on the main thread (see above). */
#define ENABLE_RENDERING_THREAD 1

/* Trace logs cost a little speed on every chip access; BMNeXT has no
   command line to turn them on. */
#undef ENABLE_TRACING

#endif /* BMNEXT_PREVIOUS_CONFIG_H_ */
