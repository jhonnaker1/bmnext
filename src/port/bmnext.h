/*
 * bmnext.h - shared declarations for the BMNeXT Previous port.
 *
 * This file is part of BMNeXT, a bare metal NeXT emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#ifndef BMNEXT_PORT_H_
#define BMNEXT_PORT_H_

#include <stdint.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* USB HID usage codes (keyboard page) that BMNeXT itself uses. */
enum {
	USAGE_A = 0x04,
	USAGE_RETURN = 0x28, USAGE_ESCAPE = 0x29, USAGE_BACKSPACE = 0x2a,
	USAGE_TAB = 0x2b, USAGE_SPACE = 0x2c,
	USAGE_F1 = 0x3a, USAGE_F4 = 0x3d, USAGE_F10 = 0x43, USAGE_F11 = 0x44,
	USAGE_F12 = 0x45,
	USAGE_PAUSE = 0x48, USAGE_HOME = 0x4a, USAGE_PAGEUP = 0x4b,
	USAGE_DELETE = 0x4c, USAGE_END = 0x4d, USAGE_PAGEDOWN = 0x4e,
	USAGE_RIGHT = 0x4f, USAGE_LEFT = 0x50, USAGE_DOWN = 0x51, USAGE_UP = 0x52,
	USAGE_KP_ENTER = 0x58, USAGE_POWER = 0x66,
	USAGE_LCTRL = 0xe0, USAGE_LSHIFT = 0xe1, USAGE_LALT = 0xe2,
	USAGE_RCTRL = 0xe4, USAGE_RSHIFT = 0xe5, USAGE_RALT = 0xe6
};

/* F12 opens the menu (as Previous's own F12 opens its settings, and the
   family's F12 its menu). */
#define KEY_MENU   USAGE_F12

/* ---- platform.c --------------------------------------------------------- */

void bmnext_main(void);
const char *bmnext_sd_root(void);
/* PATH in the card's DIR folder ("" for the root). */
void bmnext_card_path(char *path, unsigned size, const char *dir, const char *name);
/* The frame buffer shown on the display (RGB565), for the on-screen
   display. */
uint16_t *bmnext_framebuffer(int *pitch_px);
/* Shows the on-screen display's 640x460 area (TRUE) or the emulator's
   picture (FALSE). */
void bmnext_osd_view(int on);
/* Called while a menu waits: keeps sound fed and runs network requests. */
void bmnext_idle(void);
double bmnext_refresh_rate(void);
int bmnext_vsync_locked(void);
void bmnext_video_settings_changed(void);
/* The display's resolution setting changed (the Pi 5). */
void bmnext_display_mode_changed(void);
/* Reboot or power off, closing files first. */
void bmnext_request_power(int power_off);
/* Web UI requests waiting to be done; menus close for them. */
int bmnext_pending_requests(void);
/* The disk activity line (disk_activity): on or off. */
int bmnext_disk_activity(void);
void bmnext_set_disk_activity(int on);
/* Called 200 times a second of the NeXT's time (gui_event.c): shows the
   NeXT's display when it's time, opens the menu, runs requests. */
void bmnext_tick(void);
/* The NeXT switched itself off (or halted): shows TEXT, and waits for a
   key to start it again. */
void bmnext_wait_restart(const char *text);

/* Boots the machine again, as switching it off and on does (Previous's
   cold reset), or presses its reset button (warm reset). */
void bmnext_cold_reset(void);
void bmnext_warm_reset(void);

/* Previous's settings (ConfigureParams) as they are when a menu is
   opened, and putting changes made since into effect: Previous works out
   what has to be reset (Change_CopyChangedParamsToConfiguration(), as its
   own settings dialog does), and the changes are saved in previous.cfg. */
void bmnext_settings_begin(void);
void bmnext_settings_apply(int force_reset);

/* Looks for the machine's ROM in the card's roms folder if the one in the
   settings isn't there. */
void bmnext_find_roms(void);

/* ---- main_next.c ---------------------------------------------------------- */

/* Starts Previous with the settings in CONFIG_FILE, after SETTINGS (if not
   NULL) has made BMNeXT's choices, and runs it. Returns only if it can't
   go on, with the reason in *WHY. */
void bmnext_previous_run(const char *config_file, void (*settings)(void), const char **why);
/* Stops Previous: it can't go on, for WHY. */
void bmnext_stop(const char *why);
/* How fast the emulation runs, in percent of the real machine's speed. */
int bmnext_speed_percent(void);

/* Where the emulator's core spends its time besides the NeXT's CPU, for
   the log (platform.c): seconds, added up since the last report. */
enum { PROF_FRAMES, PROF_NETWORK, PROF_SOUND, PROF_FILES, PROF_SLEEP, PROF_KINDS };
extern double bmnext_prof[PROF_KINDS];

/* ---- enet_next.c (or nonet.c) --------------------------------------- */

/* The NeXT's network: SLiRP's connections and timers, 200 times a second
   (bmnext_tick()). */
void enet_next_poll(void);
/* Previous's NFS, NetInfo and other servers' sockets (rpc_sockets.c). */
void rpc_sockets_poll(void);

/* ---- menu.c ---------------------------------------------------------------- */

void bmnext_menu(void);
/* The machine emulated: "NeXTstation Turbo". */
const char *bmnext_machine_text(void);

/* ---- audio.c ---------------------------------------------------------------- */

/* Opens the sound output; returns the rate in use (0 for none). */
unsigned bmnext_sound_open(void);
void bmnext_feed_silence(void);
/* The sound volume, 0-100 (kept in bmnext.cfg). */
int bmnext_volume(void);
void bmnext_set_volume(int volume);

/* ---- screen.c ---------------------------------------------------------------- */

/* The frame buffer's size: the NeXT's MegaPixel Display. */
#define BMNEXT_FB_W 1120
#define BMNEXT_FB_H 832

/* Sets up the frame buffer: RGB565, BMNEXT_FB_W x BMNEXT_FB_H. */
void bmnext_screen_init(uint16_t *pixels, int pitch_px);
/* Copies the NeXT's display to the frame buffer, what changed of it (or
   all of it, FORCE). Returns TRUE if anything was copied. */
int bmnext_screen_copy(int force);
/* The picture's size, and the display's aspect ratio for it. */
void bmnext_screen_view(int *w, int *h, double *aspect);
/* Lines at the bottom of the picture the notification and disk light are
   drawn over: they are copied again next time. */
void bmnext_screen_dirty_bottom(void);

/* ---- keymap.c ---------------------------------------------------------------- */

/* A key (USB HID usage) pressed or released on the NeXT's keyboard. */
void bmnext_key(int usage, int pressed);
/* Releases every key held on the NeXT's keyboard (before a menu opens),
   through the queue (gui_event.c). */
void bmnext_release_keys(void);
/* The mouse moved (DX, DY) and its buttons (bit 0 left, 1 right). */
void bmnext_mouse(int dx, int dy, int buttons);

/* ---- gui_event.c --------------------------------------------------------- */

/* TRUE if the menu should open (F12, a gamepad's Home). */
int bmnext_menu_requested(void);
/* A key for the NeXT, given to it in its turn: one each time Previous asks
   (200 a second), as its keyboard holds one event at a time. */
void bmnext_queue_key(int usage, int pressed);
void bmnext_drop_queued_keys(void);

/* ---- statusbar.c --------------------------------------------------------- */

/* Draws the disk activity line over the picture. */
void bmnext_draw_disk_activity(uint16_t *fb, int pitch_px, int w, int h);

#endif /* BMNEXT_PORT_H_ */
