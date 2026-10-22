/*
 * posix_extra.c - POSIX functions Previous uses that circle-newlib lacks.
 *
 * timegm(): the NeXT's real time clock is set from the Pi's time, and read
 * back, in UTC (timing.c).
 *
 * scandir(), alphasort(): listing folders. Previous's own versions
 * (scandir.c) need dirent's d_reclen, which newlib's doesn't have.
 *
 * utime(), access(), chmod(): file dates, checks and permissions. FatFs
 * could set a file's date, but Circle's file system calls don't offer it;
 * everything on the SD card can be read and written (from BMCoco); a file
 * can't be made read only.
 *
 * Only the Pi needs these: the PC test harness (tools/hostsim) has its C
 * library's. On a Mac, the harness needs Paths_GetMacScreenShotDir()
 * instead, which Previous's Mac front end has (gui-osx/paths.m).
 *
 * This file is part of BMNeXT, a bare metal NeXT emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#ifndef BMNEXT_HOST_LIBC

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

int alphasort(const struct dirent **a, const struct dirent **b)
{
	return strcmp((*a)->d_name, (*b)->d_name);
}

/* Days since 1970-01-01 of a date in the proleptic Gregorian calendar
   (Howard Hinnant's days_from_civil). */
static long days_from_civil(long y, unsigned m, unsigned d)
{
	long era;
	unsigned yoe, doy, doe;
	y -= m <= 2;
	era = (y >= 0 ? y : y - 399) / 400;
	yoe = (unsigned) (y - era * 400);
	doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return era * 146097 + (long) doe - 719468;
}

time_t timegm(struct tm *tm)
{
	long year = tm->tm_year + 1900L + tm->tm_mon / 12;
	int mon = tm->tm_mon % 12;
	if (mon < 0) {
		mon += 12;
		year--;
	}
	return (time_t) days_from_civil(year, (unsigned) mon + 1, 1) * 86400
	       + (time_t) (tm->tm_mday - 1) * 86400 + tm->tm_hour * 3600L
	       + tm->tm_min * 60L + tm->tm_sec;
}

int scandir(const char *dirname, struct dirent ***namelist,
            int (*filter)(const struct dirent *),
            int (*compare)(const struct dirent **, const struct dirent **))
{
	DIR *dir;
	struct dirent *entry, **list = NULL;
	size_t count = 0, allocated = 0;

	*namelist = NULL;
	dir = opendir(dirname);
	if (dir == NULL)
		return -1;
	while ((entry = readdir(dir)) != NULL) {
		struct dirent *copy;
		if (filter != NULL && !filter(entry))
			continue;
		if (count == allocated) {
			size_t n = allocated ? allocated * 2 : 32;
			struct dirent **bigger = realloc(list, n * sizeof(*list));
			if (bigger == NULL)
				goto fail;
			list = bigger;
			allocated = n;
		}
		copy = malloc(sizeof(*copy));
		if (copy == NULL)
			goto fail;
		memcpy(copy, entry, sizeof(*copy));
		list[count++] = copy;
	}
	closedir(dir);
	if (compare != NULL && count > 1)
		qsort(list, count, sizeof(*list),
		      (int (*)(const void *, const void *)) compare);
	*namelist = list;
	return (int) count;

fail:
	closedir(dir);
	while (count > 0)
		free(list[--count]);
	free(list);
	errno = ENOMEM;
	return -1;
}

int utime(const char *path, const struct utimbuf *times)
{
	(void) path;
	(void) times;
	errno = ENOSYS;
	return -1;
}

int access(const char *path, int mode)
{
	struct stat st;
	(void) mode;
	return stat(path, &st);
}

int chmod(const char *path, mode_t mode)
{
	(void) path;
	(void) mode;
	errno = ENOSYS;
	return -1;
}

#elif defined(__APPLE__)

/* Screenshots go to the folder Previous runs in (paths.c, when this is NULL). */
char *Paths_GetMacScreenShotDir(void);
char *Paths_GetMacScreenShotDir(void)
{
	return NULL;
}

#endif /* BMNEXT_HOST_LIBC */
