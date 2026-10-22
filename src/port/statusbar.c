/*
 * statusbar.c - Previous's statusbar functions for BMNeXT: the disk
 * activity line.
 *
 * Replaces Previous's src/gui-sdl/sdlstatusbar.c, which draws a bar below
 * the NeXT's display. BMNeXT has none; the lights Previous sets here (the
 * SCSI disks', the optical drive's, the floppy drive's and the network's)
 * are shown instead at the bottom left of the picture, as in BMCoco,
 * BMApple2 and BMAtariST, for a moment after each access. The
 * disk_activity option (Video, Show Disk Activity) turns it off.
 *
 * Previous's messages (Statusbar_AddMessage()) go to the log.
 *
 * This file is part of BMNeXT, a bare metal NeXT emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

const char Statusbar_fileid[] = "BMNeXT statusbar.c";

#include "main.h"
#include "statusbar.h"

#include "../host.h"
#include "bmnext.h"
#include "osd.h"

static const char *const led_names[NUM_DEVICE_LEDS] = {
	"Ethernet", "Optical disk", "SCSI", "Floppy"
};

static double led_until[NUM_DEVICE_LEDS];
static double shown_until;
static int shown_led = -1;

void Statusbar_BlinkLed(drive_index_t drive)
{
	if (drive < NUM_DEVICE_LEDS)
		led_until[drive] = host_time() + 0.1;
}

void Statusbar_SetSystemLed(bool state)
{
}

void Statusbar_SetDspLed(bool state)
{
}

void Statusbar_SetNdLed(int state)
{
}

void Statusbar_UpdateInfo(void)
{
}

void Statusbar_AddMessage(const char *msg, uint32_t msecs)
{
	if (msg != NULL && *msg != '\0')
		printf("Previous: %s\n", msg);
}

/* ---- The disk activity line ----------------------------------------------- */

void bmnext_draw_disk_activity(uint16_t *fb, int pitch_px, int w, int h)
{
	double now;
	int i;
	if (!bmnext_disk_activity())
		return;
	now = host_time();
	/* The network's light is left out: the network isn't there yet. */
	for (i = DEVICE_LED_OD; i < NUM_DEVICE_LEDS; i++) {
		if (now <= led_until[i]) {
			shown_led = i;
			shown_until = now + 1.0;
			break;
		}
	}
	if (shown_led < 0 || now > shown_until)
		return;
	osd_draw_disk_status(fb, pitch_px, w, h, led_names[shown_led],
	                     now <= led_until[shown_led], FALSE);
	bmnext_screen_dirty_bottom();
}
