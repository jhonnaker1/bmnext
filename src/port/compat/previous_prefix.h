/*
 * previous_prefix.h - included before every Previous source file BMNeXT
 * builds (-include, see the Makefile).
 *
 * Some of Previous's files count on <inttypes.h>, <stdbool.h> or <stdio.h>
 * having been included before them, which other builds happen to do; they
 * are included here first, so every file builds on its own. And on the
 * Pi, scandir() and alphasort(), which newlib's <dirent.h> doesn't declare,
 * and timegm(), which its <time.h> doesn't (they are posix_extra.c's).
 * And tmpfile() for all builds, which is posix_extra.c's too.
 *
 * This file is part of BMNeXT, a bare metal NeXT emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#ifndef BMNEXT_PREVIOUS_PREFIX_H_
#define BMNEXT_PREVIOUS_PREFIX_H_

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef BMNEXT_HOST_LIBC
struct dirent;
int scandir(const char *dirname, struct dirent ***namelist,
            int (*filter)(const struct dirent *),
            int (*compare)(const struct dirent **, const struct dirent **));
int alphasort(const struct dirent **a, const struct dirent **b);
#include <time.h>
time_t timegm(struct tm *tm);
#endif

/* Temporary files are in memory (posix_extra.c). */
FILE *bmnext_tmpfile(void);
#define tmpfile() bmnext_tmpfile()

#endif /* BMNEXT_PREVIOUS_PREFIX_H_ */
