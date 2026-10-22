/*
 * platform.c - Previous platform layer for BMNeXT.
 *
 * Does what Previous's SDL front end (src/gui-sdl/, main.c) does, on top of
 * the Circle kernel services in src/host.h: starts Previous (main_next.c),
 * shows the NeXT's display, opens the menu, and keeps files written to the
 * SD card. Previous runs its CPU without stopping; 200 times a second of the
 * NeXT's time it calls the front end (gui_event.c, then bmnext_tick() here),
 * which reads the keyboard and mouse and, once a display refresh, copies
 * the NeXT's display to the frame buffer (screen.c). Previous keeps the
 * NeXT to real time itself, sleeping when it is ahead (timing.c).
 *
 * The sound is in audio.c, the keyboard and mouse in keymap.c, the menu in
 * menu.c and osd.c.
 *
 * Settings: Previous's own (machine, ROM, memory, disks...) are kept in
 * previous.cfg in the card's root, which Previous reads and writes itself;
 * BMNeXT's (video, sound volume, network) in bmnext.cfg.
 *
 * ROMs: the NeXT's ROMs are Apple's (NeXT's) and don't come with BMNeXT.
 * They go in the card's roms folder; when the one previous.cfg names isn't
 * there, the folder is searched (bmnext_find_roms()).
 *
 * This file is part of BMNeXT, a bare metal NeXT emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#include "main.h"
#include "configuration.h"
#include "change.h"
#include "file.h"
#include "reset.h"

#include <ctype.h>
#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../host.h"
#include "bmnext.h"
#include "osd.h"

/* ---- Settings ---------------------------------------------------------------- */

const char *bmnext_sd_root(void)
{
	return host_option("root", "/");
}

void bmnext_card_path(char *path, unsigned size, const char *dir, const char *name)
{
	const char *root = bmnext_sd_root();
	size_t len = strlen(root);
	snprintf(path, size, "%s%s%s%s%s", root, len > 0 && root[len - 1] == '/' ? "" : "/",
	         dir, dir[0] ? "/" : "", name);
}

static int option_on(const char *name, int default_value)
{
	const char *value = host_option(name, NULL);
	if (value == NULL || *value == '\0')
		return default_value;
	return strcmp(value, "1") == 0 || strcmp(value, "on") == 0
	    || strcmp(value, "yes") == 0 || strcmp(value, "true") == 0;
}

/* ---- Video ------------------------------------------------------------------- */

static uint16_t *fb_pixels;
static int fb_pitch_px;
static int osd_view;

static double refresh_rate = 60.0;

uint16_t *bmnext_framebuffer(int *pitch_px)
{
	*pitch_px = fb_pitch_px;
	return fb_pixels;
}

static void apply_view(void)
{
	int w, h;
	double aspect;
	if (osd_view) {
		host_video_set_view(0, 0, 640, 460, 640.0 / 460.0);
		return;
	}
	bmnext_screen_view(&w, &h, &aspect);
	host_video_set_view(0, 0, w, h, aspect * host_option_double("aspect", 1.0));
}

void bmnext_osd_view(int on)
{
	osd_view = on;
	apply_view();
	if (!on)
		bmnext_screen_copy(TRUE);
}

static void clear_framebuffer(void)
{
	int y;
	for (y = 0; y < BMNEXT_FB_H; y++)
		memset(fb_pixels + y * fb_pitch_px, 0, BMNEXT_FB_W * 2);
}

static void measure_refresh_rate(void)
{
	enum { SKIP = 3, COUNT = 20 };
	double start = 0.0, rate;
	int i;
	for (i = 0; i < SKIP + COUNT; i++) {
		host_video_present(TRUE);
		if (i == SKIP - 1)
			start = host_time();
	}
	rate = COUNT / (host_time() - start);
	if (rate > 20.0 && rate < 250.0)
		refresh_rate = rate;
	printf("BMNeXT: display refresh rate %.3f Hz\n", refresh_rate);
}

double bmnext_refresh_rate(void)
{
	return refresh_rate;
}

/* The NeXT's display (68 Hz) is never locked to the Pi's: frames are shown
   at the display's rate, the emulation paced by Previous's own clock. */
int bmnext_vsync_locked(void)
{
	return FALSE;
}

void bmnext_video_settings_changed(void)
{
	if (fb_pixels == NULL)
		return;
	clear_framebuffer();
	apply_view();
	bmnext_screen_copy(TRUE);
}

void bmnext_display_mode_changed(void)
{
	/* Where the host can set the video mode (the Pi 4 and 5), the
	   display runs at 60 Hz: the NeXT's 68 Hz is unusual for a TV. */
	if (host_display_match_rate(60.0))
		measure_refresh_rate();
	bmnext_sound_open();
	apply_view();
}

/* ---- Disk activity -------------------------------------------------------- */

static int da_enabled = -1;

int bmnext_disk_activity(void)
{
	if (da_enabled < 0)
		da_enabled = host_option_int("disk_activity", 1) != 0;
	return da_enabled;
}

void bmnext_set_disk_activity(int on)
{
	da_enabled = on != 0;
}

/* ---- Files ---------------------------------------------------------------- */

/* Previous writes the NeXT's disks straight to their images on the card.
   The Pi can be switched off at any time: everything is pushed out to the
   card once a second. */
static void sync_files(void)
{
	int fd;
	fflush(NULL);
	for (fd = 3; fd < 30; fd++)
		fsync(fd);
}

/* ---- ROMs ---------------------------------------------------------------- */

/* A NeXT ROM in the card's roms folder (or its root): 64K for the 68030
   NeXT Computer (ROM 0.8 to 1.2), 128K for the 68040 machines (ROM 2.x)
   and the Turbos (ROM 3.x). Which 128K ROM is which is told by its name
   (Previous's own are Rev_2.5_v66.BIN and Rev_3.3_v74.BIN). */
enum { ROM_030, ROM_040, ROM_TURBO, ROM_KINDS };

static int rom_kind(const char *name, long size)
{
	const char *p;
	if (size == 64 * 1024)
		return ROM_030;
	if (size != 128 * 1024)
		return -1;
	for (p = name; *p; p++) {
		if (p[0] == '3' && (p[1] == '.' || p[1] == '_'))
			return ROM_TURBO;
		if (p[0] == '2' && (p[1] == '.' || p[1] == '_'))
			return ROM_040;
	}
	if (strstr(name, "v74") || strstr(name, "V74"))
		return ROM_TURBO;
	return ROM_040;
}

static int has_rom_ext(const char *name)
{
	const char *dot = strrchr(name, '.');
	return dot != NULL && (strcasecmp(dot, ".bin") == 0 || strcasecmp(dot, ".rom") == 0);
}

static char found_rom[ROM_KINDS][FILENAME_MAX];

static void scan_roms(void)
{
	static const char *const dirs[] = {"roms", ""};
	unsigned d;
	memset(found_rom, 0, sizeof found_rom);
	for (d = 0; d < sizeof dirs / sizeof dirs[0]; d++) {
		char dir_path[FILENAME_MAX];
		DIR *dir;
		struct dirent *e;
		bmnext_card_path(dir_path, sizeof dir_path, dirs[d], "");
		dir = opendir(dir_path);
		if (dir == NULL)
			continue;
		while ((e = readdir(dir)) != NULL) {
			char path[FILENAME_MAX];
			struct stat st;
			int kind;
			if (e->d_name[0] == '.' || !has_rom_ext(e->d_name))
				continue;
			bmnext_card_path(path, sizeof path, dirs[d], e->d_name);
			if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
				continue;
			kind = rom_kind(e->d_name, (long) st.st_size);
			/* The newest of each kind (by name: Rev_1.2 after Rev_1.0). */
			if (kind >= 0 && (found_rom[kind][0] == '\0'
			                  || strcasecmp(path, found_rom[kind]) > 0))
				snprintf(found_rom[kind], sizeof found_rom[kind], "%s", path);
		}
		closedir(dir);
	}
}

static void use_rom(char *setting, int kind, const char *what)
{
	if (File_Exists(setting))
		return;
	if (found_rom[kind][0] == '\0') {
		printf("BMNeXT: no %s ROM in roms/ (see roms/README.txt)\n", what);
		return;
	}
	printf("BMNeXT: %s ROM %s\n", what, found_rom[kind]);
	snprintf(setting, FILENAME_MAX, "%s", found_rom[kind]);
}

void bmnext_find_roms(void)
{
	scan_roms();
	use_rom(ConfigureParams.Rom.szRom030FileName, ROM_030, "68030");
	use_rom(ConfigureParams.Rom.szRom040FileName, ROM_040, "68040");
	use_rom(ConfigureParams.Rom.szRomTurboFileName, ROM_TURBO, "Turbo");
}

/* BMNeXT's choices over previous.cfg's, before Previous starts. */
static void startup_settings(void)
{
	int i;
	bmnext_find_roms();
	/* No NeXTdimension (nextbus.c). */
	for (i = 0; i < ND_MAX_BOARDS; i++)
		ConfigureParams.Dimension.board[i].bEnabled = false;
	ConfigureParams.Dimension.bI860Thread = false;
	/* Previous's own settings dialog doesn't come up (it's BMNeXT's menu),
	   nor its statusbar. */
	ConfigureParams.ConfigDialog.bShowConfigDialogAtStartup = false;
	ConfigureParams.Screen.bShowStatusbar = false;
	ConfigureParams.Screen.nMode = SCREEN_SINGLE;
	ConfigureParams.Screen.nSingleModeSlot = 0;
	ConfigureParams.Sound.bEnableSound = option_on("sound", TRUE);
}

/* ---- Settings changes ----------------------------------------------------- */

static CNF_PARAMS settings_before;

void bmnext_settings_begin(void)
{
	settings_before = ConfigureParams;
}

void bmnext_settings_apply(int force_reset)
{
	/* Previous's own settings dialog's way (dialog.c): it works out what
	   has to be reset or reloaded for the changes. */
	Change_CopyChangedParamsToConfiguration(&settings_before, &ConfigureParams,
	                                        force_reset);
	Configuration_Save();
	settings_before = ConfigureParams;
	bmnext_video_settings_changed();
}

/* ---- Resets ------------------------------------------------------------------- */

void bmnext_cold_reset(void)
{
	Reset_Cold();
}

void bmnext_warm_reset(void)
{
	Reset_Warm();
}

/* ---- Requests from the web UI ------------------------------------------ */

static volatile int request_power = -1;   /* 0 reboot, 1 power off */
static volatile int request_coldstart = FALSE;
static char autostart_path[600];
static volatile int request_autostart = FALSE;

void bmnext_request_power(int power_off)
{
	request_power = power_off ? 1 : 0;
}

/* Called by the web UI. */
void bmnext_request_coldstart(void)
{
	request_coldstart = TRUE;
}

/* Called by the web UI ("start this file"), on core 0: the emulator takes
   it up at its next tick (handle_requests()). One at a time. */
void emu_autostart_interrupt(const char *path)
{
	if (request_autostart)
		return;
	snprintf(autostart_path, sizeof autostart_path, "%s", path);
	__sync_synchronize();
	request_autostart = TRUE;
}

int bmnext_pending_requests(void)
{
	return request_power >= 0 || request_coldstart || request_autostart;
}

/* The web UI's "start this file": a disk image becomes the NeXT's first
   SCSI hard disk, and the NeXT starts again from it. */
static void autostart(const char *path)
{
	SCSIDISK *t = &ConfigureParams.SCSI.target[0];
	printf("Web UI: starting %s\n", path);
	bmnext_settings_begin();
	snprintf(t->szImageName, sizeof t->szImageName, "%s", path);
	t->nDeviceType = SD_HARDDISK;
	t->bDiskInserted = true;
	ConfigureParams.Boot.nBootDevice = BOOT_SCSI;
	bmnext_settings_apply(TRUE);
}

static void power(int off)
{
	printf("%s\n", off ? "Shutting down" : "Rebooting");
	sync_files();
	if (off)
		host_power_off();
	else
		host_reboot();
}

static void handle_requests(void)
{
	if (request_autostart) {
		__sync_synchronize();
		autostart(autostart_path);
		request_autostart = FALSE;
	}
	if (request_coldstart) {
		request_coldstart = FALSE;
		printf("Web UI: restart\n");
		bmnext_cold_reset();
	}
	if (request_power >= 0) {
		int off = request_power;
		request_power = -1;
		power(off);
	}
}

static double last_sync_time;

void bmnext_idle(void)
{
	double now = host_time();
	bmnext_feed_silence();
	if (now - last_sync_time >= 1.0) {
		last_sync_time = now;
		sync_files();
	}
	if (request_power >= 0)
		handle_requests();
}

/* ---- Running ------------------------------------------------------------- */

static void show_network_address(void)
{
	static int shown;
	char address[32];
	if (shown || !option_on("webui", TRUE))
		return;
	if (host_network_address(address, sizeof(address))) {
		char text[64];
		snprintf(text, sizeof(text), "Web UI: http://%s/", address);
		osd_notify(text);
		shown = TRUE;
	}
}

static double next_frame;
static double last_second;

static void show_frame(void)
{
	if (osd_view)
		return;
	bmnext_screen_copy(FALSE);
	bmnext_draw_disk_activity(fb_pixels, fb_pitch_px, BMNEXT_FB_W, BMNEXT_FB_H);
	if (osd_draw_notification(fb_pixels, fb_pitch_px, BMNEXT_FB_W, BMNEXT_FB_H))
		bmnext_screen_dirty_bottom();
	host_video_present(FALSE);
}

void bmnext_tick(void)
{
	double now = host_time();
	if (now >= next_frame) {
		show_frame();
		next_frame += 1.0 / refresh_rate;
		if (now - next_frame > 0.25 || next_frame - now > 0.25)
			next_frame = now + 1.0 / refresh_rate;
	}
	if (bmnext_menu_requested()) {
		bmnext_release_keys();
		Main_PauseEmulation(false);
		bmnext_menu();
		Main_UnPauseEmulation();
		apply_view();
		bmnext_screen_copy(TRUE);
		next_frame = host_time();
	}
	if (now - last_second >= 1.0) {
		last_second = now;
		sync_files();
		show_network_address();
	}
	handle_requests();
}

/* Waits for a key (or the menu, or a request from the web UI). */
static void wait_for_key(void)
{
	int usage, pressed;
	for (;;) {
		bmnext_idle();
		host_video_present(TRUE);
		while (host_key_event(&usage, &pressed)) {
			if (!pressed)
				continue;
			if (usage == KEY_MENU) {
				osd_close();
				bmnext_menu();
				return;
			}
			return;
		}
		if (bmnext_pending_requests())
			return;
	}
}

void bmnext_wait_restart(const char *text)
{
	osd_open();
	osd_message(text, FALSE);
	wait_for_key();
	if (osd_is_open())
		osd_close();
	bmnext_release_keys();
	handle_requests();
}

static void stopped(const char *why)
{
	char text[FILENAME_MAX + 160];
	snprintf(text, sizeof text, "%s. The log (see the User Guide) says more.", why);
	osd_open();
	osd_message(text, FALSE);
	for (;;) {
		bmnext_idle();
		host_video_present(TRUE);
	}
}

void bmnext_main(void)
{
	static char previous_cfg[FILENAME_MAX];
	const char *why = "Previous stopped";
	int pitch;

	if (host_video_init(BMNEXT_FB_W, BMNEXT_FB_H, &fb_pixels, &pitch) != 0) {
		printf("BMNeXT: no display\n");
		for (;;)
			host_sleep_us(1000000);
	}
	fb_pitch_px = pitch / 2;
	clear_framebuffer();
	bmnext_screen_init(fb_pixels, fb_pitch_px);
	apply_view();
	host_display_match_rate(60.0);
	measure_refresh_rate();
	bmnext_sound_open();

	/* Previous's home folder (where it would keep its files) is the card. */
	setenv("HOME", bmnext_sd_root(), 1);
	bmnext_card_path(previous_cfg, sizeof previous_cfg, "", "previous.cfg");

	next_frame = last_second = host_time();
	bmnext_previous_run(previous_cfg, startup_settings, &why);
	stopped(why);
}
