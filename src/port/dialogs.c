/*
 * dialogs.c - Previous's dialogs, for BMNeXT.
 *
 * Previous's SDL front end has its own settings dialogs (src/gui-sdl/dlg*.c,
 * its F12); BMNeXT's menu (menu.c) does that instead. What Previous asks
 * its dialogs for while it runs is answered here:
 *
 *   a question (DlgAlert_Query)   no; the question goes to the log
 *   a notice                      the notification line
 *   a missing ROM                 Previous stops (main_next.c shows why):
 *                                 platform.c has already looked for one
 *                                 on the card
 *   a missing disk image          that drive is left empty
 *   a missing folder              the card's root
 *
 * This file is part of BMNeXT, a bare metal NeXT emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#include "main.h"
#include "configuration.h"
#include "dialog.h"
#include "paths.h"
#include "str.h"

#include "../host.h"
#include "bmnext.h"
#include "osd.h"

int Dialog_MainDlg(bool *bReset, bool *bLoadedSnapshot)
{
	*bReset = false;
	*bLoadedSnapshot = false;
	return 0;
}

bool DlgAlert_Query(const char *text)
{
	printf("Previous asks: %s (no)\n", text);
	return false;
}

bool DlgAlert_Notice(const char *text)
{
	printf("Previous: %s\n", text);
	osd_notify(text);
	return true;
}

void DlgMissing_Rom(const char *type, char *imgname, const char *defname, bool *enabled)
{
	char text[FILENAME_MAX + 80];
	snprintf(text, sizeof text, "No ROM for the %s: %s is not there", type, imgname);
	bmnext_stop(text);
	if (enabled)
		*enabled = false;
}

void DlgMissing_Dir(const char *type, char *dirname, const char *defname)
{
	printf("BMNeXT: the %s folder %s is not there, using %s\n", type, dirname, defname);
	Str_Copy(dirname, defname, FILENAME_MAX);
}

void DlgMissing_Disk(const char *type, int num, char *imgname, bool *ins, bool *wp)
{
	char text[FILENAME_MAX + 80];
	snprintf(text, sizeof text, "%s %d: %s is not there", type, num, imgname);
	printf("BMNeXT: %s\n", text);
	osd_notify(text);
	*ins = false;
	imgname[0] = '\0';
}
