/*
 * screen.c - Previous's screen functions for BMNeXT.
 *
 * Replaces Previous's src/gui-sdl/sdlscreen.c. The NeXT's display is
 * 1120x832, in one of two formats in the NeXT's video memory (NEXTVideo):
 *
 *   monochrome  2 bits a pixel (four greys, 0 white), four pixels a byte
 *   colour      16 bits a pixel, big endian: 4 bits each of red, green,
 *               blue and a fourth Previous ignores
 *
 * with 32 pixels more on each line, unshown, on the machines that are not
 * Turbos. SDL's front end converts the picture on a thread of its own, as
 * fast as it can; here the platform layer copies it to the display's RGB565
 * frame buffer (bmnext_screen_copy()), once each display refresh. Only the
 * lines that changed since the last copy are converted, which on the
 * NeXT's mostly still desktop is few of them.
 *
 * A NeXTcube can also have a NeXTdimension board in slot 2, with a screen
 * of its own: 1120x832 too, 32 bits a pixel (blue, green, red and alpha
 * bytes), 1152 pixels a line, in the board's video memory
 * (nd_vram_for_slot()). One of the two screens is shown (Video > Screen):
 * by default (Auto) the NeXT's until the board's has a picture: the
 * board's ROM switches its video on with the screen one plain colour,
 * NeXTSTEP's window server then draws the desktop there; from then on the
 * board's, until the NeXT restarts. For now its 24-bit colour is
 * shown as the display's RGB565, like the NeXT's own.
 *
 * Previous's statusbar is not used: the disk activity is drawn by BMNeXT
 * (statusbar.c), and the menu is BMNeXT's.
 *
 * This file is part of BMNeXT, a bare metal NeXT emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#include "main.h"
#include "configuration.h"
#include "screen.h"
#include "statusbar.h"
#include "video.h"
#include "dimension.hpp"

#include "../host.h"
#include "bmnext.h"

/* From Previous's cpu/memory.h, which brings in the whole CPU core. */
extern uint8_t *NEXTVideo;

const int NeXT_SCRN_W = BMNEXT_FB_W;
const int NeXT_SCRN_H = BMNEXT_FB_H;

volatile bool bGrabMouse = true;      /* the mouse is always the NeXT's */
volatile bool bInFullScreen = true;   /* the display is the NeXT's */

/* For the tablet (tablet.c). */
int screen_w = BMNEXT_FB_W;
int screen_h = BMNEXT_FB_H;

static uint16_t *fb;                  /* the display's */
static int fb_pitch;

/* What the last copy saw of each line (of NEXTVideo or the board's
   memory), to find what changed. */
static uint8_t *last_lines;
static int last_valid;
static int last_color = -1, last_turbo = -1, last_source = -1;

/* The NeXTdimension board's slot (BMNeXT's one board), and its lines. */
#define ND_BOARD_SLOT ND_SLOT(0)
#define ND_LINE_PX (BMNEXT_FB_W + 32)

static int nd_screen = BMNEXT_SCREEN_AUTO;
/* Auto: whether the board's screen has had more than one colour since the
   NeXT started, and when it was last looked at. */
static int nd_has_picture;
static int nd_look;

/* Four monochrome pixels (a byte) at a time, and the colour format. */
static uint16_t bw_rgb[256][4];
static uint16_t col_rgb[65536];

static uint16_t rgb565(int r, int g, int b)
{
	return (uint16_t) (((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3));
}

static void make_tables(void)
{
	int i, p;
	for (i = 0; i < 256; i++) {
		for (p = 0; p < 4; p++) {
			int bw = (i >> (6 - 2 * p)) & 3;
			int c = (~bw & 3) * 0x55;
			bw_rgb[i][p] = rgb565(c, c, c);
		}
	}
	/* Indexed by the 16-bit word as it is in memory (big endian), read
	   as a native one. */
	for (i = 0; i < 65536; i++) {
		const uint8_t be[2] = {(uint8_t) (i >> 8), (uint8_t) i};
		uint16_t native;
		memcpy(&native, be, 2);
		col_rgb[native] = rgb565(((i >> 12) & 15) * 0x11, ((i >> 8) & 15) * 0x11,
		                         ((i >> 4) & 15) * 0x11);
	}
}

void bmnext_screen_init(uint16_t *pixels, int pitch_px)
{
	fb = pixels;
	fb_pitch = pitch_px;
	make_tables();
}

void bmnext_set_screen(int which)
{
	nd_screen = which;
	last_valid = FALSE;
}

int bmnext_screen(void)
{
	return nd_screen;
}

/* Whether the board's screen has more than one colour, from a grid of
   its pixels. */
static int nd_picture(const uint32_t *vram)
{
	int x, y;
	uint32_t first = vram[0] & 0x00ffffffu;
	for (y = 0; y < BMNEXT_FB_H; y += BMNEXT_FB_H / 26)
		for (x = 0; x < BMNEXT_FB_W; x += BMNEXT_FB_W / 35)
			if ((vram[(size_t) y * ND_LINE_PX + x] & 0x00ffffffu) != first)
				return TRUE;
	return FALSE;
}

/* Whether the board's screen is the one to show. */
static int show_dimension(void)
{
	if (!ConfigureParams.Dimension.board[0].bEnabled
	    || nd_vram_for_slot(ND_BOARD_SLOT) == NULL)
		return FALSE;
	switch (nd_screen) {
	case BMNEXT_SCREEN_NEXT:
		return FALSE;
	case BMNEXT_SCREEN_DIMENSION:
		return TRUE;
	default:
		if (!nd_video_enabled(ND_BOARD_SLOT))
			return FALSE;
		/* Looked at twice a second until it has a picture. */
		if (!nd_has_picture && ++nd_look >= 30) {
			nd_look = 0;
			nd_has_picture = nd_picture(nd_vram_for_slot(ND_BOARD_SLOT));
		}
		return nd_has_picture;
	}
}

/* ---- Previous's screen functions ------------------------------------------ */

void Screen_EnterFullScreen(void)
{
}

void Screen_ReturnFromFullScreen(void)
{
}

void Screen_SetMouseGrab(bool grab)
{
}

void Screen_ShowMainWindow(void)
{
}

void Screen_TitlebarChanged(void)
{
}

void Screen_StatusbarMessage(const char *msg, uint32_t msecs)
{
	Statusbar_AddMessage(msg, msecs);
}

void Screen_StatusbarUpdate(void)
{
}

bool Screen_ShowCursor(bool show)
{
	return false;
}

void Screen_CenterCursor(void)
{
}

/* A new machine (or reset): copy everything again. */
void Screen_Reset(void)
{
	last_valid = FALSE;
	nd_has_picture = FALSE;
}

/* ---- To the display ----------------------------------------------------- */

/* How many lines at the bottom are drawn over (osd.c's notification and
   statusbar.c's disk light). */
#define OVERLAY_LINES 48

static int bottom_dirty;

void bmnext_screen_dirty_bottom(void)
{
	bottom_dirty = TRUE;
}

static void blank(void)
{
	int y;
	for (y = 0; y < BMNEXT_FB_H; y++)
		memset(fb + (size_t) y * fb_pitch, 0, BMNEXT_FB_W * 2);
}

/* The board's lines that changed, as RGB565. */
static int copy_dimension(const uint8_t *vram, int redraw_from)
{
	int y, x, copied = FALSE;
	const int shown = BMNEXT_FB_W * 4;
	for (y = 0; y < BMNEXT_FB_H; y++) {
		const uint8_t *src = vram + (size_t) y * ND_LINE_PX * 4;
		uint8_t *seen = last_lines + (size_t) y * shown;
		uint16_t *dst = fb + (size_t) y * fb_pitch;
		if (last_valid && y < redraw_from && memcmp(src, seen, shown) == 0)
			continue;
		memcpy(seen, src, shown);
		for (x = 0; x < BMNEXT_FB_W; x++, src += 4)
			dst[x] = rgb565(src[2], src[1], src[0]);
		copied = TRUE;
	}
	return copied;
}

int bmnext_screen_copy(int force)
{
	int color = ConfigureParams.System.bColor;
	int turbo = ConfigureParams.System.bTurbo;
	/* Bytes of a line in video memory, and of what is shown of it. */
	int shown = color ? BMNEXT_FB_W * 2 : BMNEXT_FB_W / 4;
	int stride = turbo ? shown : color ? (BMNEXT_FB_W + 32) * 2 : (BMNEXT_FB_W + 32) / 4;
	int y, copied = FALSE;
	int redraw_from;
	int source = show_dimension();

	if (fb == NULL)
		return FALSE;
	if (source != last_source) {
		last_source = source;
		last_valid = FALSE;
		force = TRUE;
	}
	if (source) {
		/* The board's screen: blank until NeXTSTEP switches it on. */
		if (!nd_video_enabled(ND_BOARD_SLOT)) {
			if (last_valid || force) {
				blank();
				last_valid = FALSE;
			}
			return force;
		}
		if (last_lines == NULL) {
			last_lines = malloc((size_t) BMNEXT_FB_W * 4 * BMNEXT_FB_H);
			if (last_lines == NULL)
				return FALSE;
		}
		if (force)
			last_valid = FALSE;
		redraw_from = bottom_dirty ? BMNEXT_FB_H - OVERLAY_LINES : BMNEXT_FB_H;
		bottom_dirty = FALSE;
		copied = copy_dimension((const uint8_t *) nd_vram_for_slot(ND_BOARD_SLOT),
		                        redraw_from);
		last_valid = TRUE;
		return copied;
	}
	if (NEXTVideo == NULL || !Video_Enabled()) {
		/* The NeXT has blanked its display (or not started). */
		if (last_valid || force) {
			blank();
			last_valid = FALSE;
		}
		return force;
	}
	if (last_lines == NULL) {
		/* Big enough for the board's lines too. */
		last_lines = malloc((size_t) BMNEXT_FB_W * 4 * BMNEXT_FB_H);
		if (last_lines == NULL)
			return FALSE;
	}
	if (color != last_color || turbo != last_turbo) {
		last_color = color;
		last_turbo = turbo;
		last_valid = FALSE;
	}
	if (force)
		last_valid = FALSE;
	redraw_from = bottom_dirty ? BMNEXT_FB_H - OVERLAY_LINES : BMNEXT_FB_H;
	bottom_dirty = FALSE;

	for (y = 0; y < BMNEXT_FB_H; y++) {
		const uint8_t *src = NEXTVideo + (size_t) y * stride;
		uint8_t *seen = last_lines + (size_t) y * shown;
		uint16_t *dst = fb + (size_t) y * fb_pitch;
		int x;
		if (last_valid && y < redraw_from && memcmp(src, seen, shown) == 0)
			continue;
		memcpy(seen, src, shown);
		if (color) {
			const uint16_t *s = (const uint16_t *) src;
			for (x = 0; x < BMNEXT_FB_W; x++)
				dst[x] = col_rgb[s[x]];
		} else {
			for (x = 0; x < BMNEXT_FB_W / 4; x++)
				memcpy(dst + x * 4, bw_rgb[src[x]], 8);
		}
		copied = TRUE;
	}
	last_valid = TRUE;
	return copied;
}

void bmnext_screen_view(int *w, int *h, double *aspect)
{
	/* The NeXT's MegaPixel Display: square pixels. */
	*w = BMNEXT_FB_W;
	*h = BMNEXT_FB_H;
	*aspect = (double) BMNEXT_FB_W / BMNEXT_FB_H;
}
