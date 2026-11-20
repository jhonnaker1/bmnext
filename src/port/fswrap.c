/*
 * fswrap.c - file system fixes for Circle's FatFs.
 *
 * FatFs cannot stat() the root directory of a volume, so the file
 * browser would think "/" doesn't exist and refuse to go up to it. The
 * kernel is linked with --wrap=stat, so every call to stat() comes here
 * first; the root (and ".") are reported as directories, everything else
 * goes to the real stat().
 *
 * Previous makes the disk and ROM paths it loads from previous.cfg
 * absolute (configuration.c, File_MakeAbsoluteName() in file.c), putting
 * the current folder ("SD:/") in front of any that doesn't start with "/".
 * A USB stick's path ("USB:/disks/x.img") names its own volume, and would
 * become "SD:/USB:/disks/x.img": with --wrap=File_MakeAbsoluteName such a
 * path is left as it is.
 *
 * This file is part of BMNeXT, a bare metal NeXT emulator for
 * the Raspberry Pi. See LICENSE for licensing details.
 */

#include <string.h>
#include <sys/stat.h>

int __real_stat(const char *path, struct stat *st);

static int is_root(const char *path)
{
	const char *colon = strchr(path, ':');
	/* Skip a volume name such as "SD:" */
	if (colon != NULL)
		path = colon + 1;
	while (*path == '/')
		path++;
	return *path == '\0' || strcmp(path, ".") == 0 || strcmp(path, "./") == 0;
}

int __wrap_stat(const char *path, struct stat *st)
{
	if (path != NULL && st != NULL && is_root(path)) {
		memset(st, 0, sizeof(*st));
		st->st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
		st->st_nlink = 1;
		return 0;
	}
	return __real_stat(path, st);
}

void __real_File_MakeAbsoluteName(char *path);

void __wrap_File_MakeAbsoluteName(char *path)
{
	const char *p = path;
	/* A volume's name ("USB:", "USB2:"), letters and digits, then ":/". */
	while ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))
		p++;
	if (p != path && p[0] == ':' && p[1] == '/')
		return;
	__real_File_MakeAbsoluteName(path);
}
