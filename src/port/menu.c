/*
 * menu.c - the BMNeXT menu.
 *
 * F12, a gamepad's Home button or Select+Start open it (Previous's own F12
 * opens its settings dialog). Everything about the emulated NeXT (its
 * disks, the machine, ROM, memory) and BMNeXT's own settings (video,
 * sound, network, power).
 *
 * The NeXT's settings are Previous's (ConfigureParams): they are changed
 * here and put into effect the way Previous's own settings dialog does it
 * (bmnext_settings_apply(), which restarts the NeXT when a change needs
 * it), and kept in previous.cfg. BMNeXT's are kept in bmnext.cfg.
 *
 * From BMCoco's, BMApple2's and BMAtariST's menu.c.
 *
 * This file is part of BMNeXT, a bare metal NeXT emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#include "main.h"
#include "configuration.h"

#include <stdint.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>

#include "../host.h"
#include "bmnext.h"
#include "osd.h"

#ifndef BMNEXT_VERSION
#define BMNEXT_VERSION "dev"
#endif

#define WPA_CONFIG "wpa_supplicant.conf"

/* Something changed that only takes effect after a reboot. */
static int reboot_needed;

/* Where the file browser was last ("" until it's first used). */
static char browse_dir[256];

/* ---- Settings ---------------------------------------------------------- */

static void save_setting(const char *name, const char *value)
{
	if (!host_option_set(name, value) || !host_options_save())
		osd_message("Cannot save bmnext.cfg", TRUE);
}

static void save_setting_int(const char *name, int value)
{
	char text[16];
	snprintf(text, sizeof(text), "%d", value);
	save_setting(name, text);
}

static int setting_on(const char *name, int default_value)
{
	const char *value = host_option(name, NULL);
	if (value == NULL || *value == '\0')
		return default_value;
	return strcmp(value, "1") == 0 || strcmp(value, "yes") == 0
	    || strcmp(value, "true") == 0 || strcmp(value, "on") == 0;
}

static unsigned check(int on)
{
	return OSD_ITEM_CHECK | (on ? OSD_ITEM_CHECKED : 0);
}

static void power(int off)
{
	bmnext_request_power(off);
	bmnext_idle();
}

static const char *base_name(const char *path)
{
	const char *slash = strrchr(path, '/');
	return slash ? slash + 1 : path;
}

/* Starts the file browser in the folder last used, or DIR on the card. */
static int browse(const char *title, const char *dir, const char *const exts[],
                  char *path, size_t size)
{
	if (browse_dir[0] == '\0')
		bmnext_card_path(browse_dir, sizeof browse_dir, dir, "");
	return osd_browse(title, browse_dir, sizeof browse_dir, exts, path, size);
}

/* ---- Disks ---------------------------------------------------------------- */

/* Previous's disk images: raw images of whole disks (a SCSI disk, a CD, an
   optical disk or a floppy), whatever their name ends in. */
static const char *const disk_exts[] = {"dsk", "img", "iso", "cdr", "raw", "hd", "od", "fd",
                                        "bin", NULL};

static const char *const scsi_types[] = {"None", "Hard Disk", "CD-ROM", "Floppy", NULL};

/* ---- Blank disks ------------------------------------------------------ */

/* The sizes of the empty disk images Previous comes with (its disks
   folder): the NeXT's own drives, which Previous recognises by their size,
   and all zeros, for NeXTSTEP's BuildDisk (or Disk.app) to label. The ones
   marked "early" are those NeXTSTEP before 2.0 recognises. Optical disks
   come formatted, so they aren't made here. */
struct blank_size {
	const char *name;
	const char *file;
	uint64_t bytes;
};

static const struct blank_size hard_disk_sizes[] = {
	{"105 MB", "blank105.sd", 110100480ull},
	{"250 MB", "blank250.sd", 262144000ull},
	{"330 MB", "blank330.sd", 346030080ull},
	{"340 MB (early NeXTSTEP)", "blank349x.sd", 366305280ull},
	{"400 MB", "blank400.sd", 419430400ull},
	{"660 MB", "blank660.sd", 692060160ull},
	{"660 MB (early NeXTSTEP)", "blank660x.sd", 692649984ull},
	{"1.4 GB", "blank1400.sd", 1468006400ull},
	{"2.8 GB", "blank2800.sd", 2936012800ull},
	{NULL, NULL, 0}
};

static const struct blank_size floppy_sizes[] = {
	{"720 KB", "blank720.fd", 737280ull},
	{"1.44 MB", "blank1440.fd", 1474560ull},
	{"2.88 MB", "blank2880.fd", 2949120ull},
	{NULL, NULL, 0}
};

/* Asks which size, and makes a blank disk image of it in the card's disks
   folder (a new name if the file is there), with progress shown: a big
   one takes a while to write to the card. Returns 1 with its path in
   PATH. */
static int create_blank(const struct blank_size *sizes, char *path, size_t size)
{
	static unsigned char zeros[256 * 1024];
	const char *names[16];
	char dir[FILENAME_MAX], name[64], text[96];
	struct stat st;
	uint64_t done = 0, total;
	FILE *f;
	int n, i, k;
	for (n = 0; sizes[n].name != NULL && n < 15; n++)
		names[n] = sizes[n].name;
	names[n] = NULL;
	i = osd_choose("Size of the Blank Disk", names, 0);
	if (i < 0)
		return 0;
	total = sizes[i].bytes;
	bmnext_card_path(dir, sizeof dir, "disks", "");
	mkdir(dir, 0777);
	snprintf(name, sizeof name, "%s", sizes[i].file);
	bmnext_card_path(path, size, "disks", name);
	for (k = 2; stat(path, &st) == 0 && k < 100; k++) {
		const char *dot = strrchr(sizes[i].file, '.');
		snprintf(name, sizeof name, "%.*s-%d%s", (int) (dot - sizes[i].file),
		         sizes[i].file, k, dot);
		bmnext_card_path(path, size, "disks", name);
	}
	f = fopen(path, "wb");
	if (f == NULL) {
		osd_message("Cannot create the disk image on the SD card.", TRUE);
		return 0;
	}
	while (done < total) {
		size_t chunk = total - done > sizeof zeros ? sizeof zeros : (size_t) (total - done);
		if (fwrite(zeros, 1, chunk, f) != chunk)
			break;
		done += chunk;
		if ((done & (32u * 1024 * 1024 - 1)) == 0 || done == total) {
			snprintf(text, sizeof text, "Writing %s: %d%%", name,
			         (int) (done * 100 / total));
			osd_message(text, FALSE);
			bmnext_idle();
		}
	}
	if (fclose(f) != 0 || done < total) {
		remove(path);
		osd_message("The SD card is full, or the disk image could not be written.", TRUE);
		return 0;
	}
	printf("BMNeXT: created %s (%s)\n", path, sizes[i].name);
	return 1;
}

static void scsi_menu(int target)
{
	enum { T_TYPE, T_INSERT, T_CREATE, T_EJECT, T_PROTECT };
	int sel = 0;
	for (;;) {
		SCSIDISK *t = &ConfigureParams.SCSI.target[target];
		char title[24];
		struct osd_item items[] = {
			{"Device", scsi_types[t->nDeviceType % NUM_SD], OSD_ITEM_SUBMENU, T_TYPE},
			{"Disk", t->bDiskInserted && t->szImageName[0] ? base_name(t->szImageName)
			                                               : "(empty)", OSD_ITEM_LABEL, -1},
			{"", NULL, OSD_ITEM_LABEL, -1},
			{"Insert a Disk Image...", NULL, OSD_ITEM_SUBMENU, T_INSERT},
			{"Create a Blank Hard Disk...", NULL, OSD_ITEM_SUBMENU, T_CREATE},
			{"Eject", NULL, 0, T_EJECT},
			{"Write Protected", NULL, check(t->bWriteProtected), T_PROTECT},
		};
		char path[FILENAME_MAX];
		int chosen, i;
		snprintf(title, sizeof title, "SCSI Disk %d", target);
		chosen = osd_menu(title, items, sizeof items / sizeof items[0], &sel);
		bmnext_settings_begin();
		switch (chosen) {
		case T_TYPE:
			i = osd_choose("Device", scsi_types, t->nDeviceType % NUM_SD);
			if (i >= 0 && i != (int) t->nDeviceType) {
				t->nDeviceType = i;
				if (i == SD_NONE)
					t->bDiskInserted = false;
				bmnext_settings_apply(i == SD_HARDDISK || i == SD_NONE);
			}
			break;
		case T_INSERT:
			if (!browse("Insert a Disk Image", "disks", disk_exts, path, sizeof path))
				break;
			snprintf(t->szImageName, sizeof t->szImageName, "%s", path);
			if (t->nDeviceType == SD_NONE)
				t->nDeviceType = SD_HARDDISK;
			t->bDiskInserted = true;
			/* Previous starts a new NeXT in its ROM monitor (with no disk
			   a NeXT can wait minutes for one to spin up): a hard disk
			   in SCSI disk 0 is started from. Machine, Boot changes it. */
			if (target == 0 && t->nDeviceType == SD_HARDDISK
			    && ConfigureParams.Boot.nBootDevice == BOOT_ROM)
				ConfigureParams.Boot.nBootDevice = BOOT_SCSI;
			bmnext_settings_apply(t->nDeviceType == SD_HARDDISK);
			break;
		case T_CREATE:
			if (!create_blank(hard_disk_sizes, path, sizeof path))
				break;
			snprintf(t->szImageName, sizeof t->szImageName, "%s", path);
			t->nDeviceType = SD_HARDDISK;
			t->bDiskInserted = true;
			t->bWriteProtected = false;
			bmnext_settings_apply(TRUE);
			break;
		case T_EJECT:
			t->bDiskInserted = false;
			bmnext_settings_apply(t->nDeviceType == SD_HARDDISK);
			break;
		case T_PROTECT:
			t->bWriteProtected = !t->bWriteProtected;
			bmnext_settings_apply(t->nDeviceType == SD_HARDDISK);
			break;
		default:
			return;
		}
		if (osd_closing())
			return;
	}
}

/* An optical (MO) or floppy drive: CONNECTED, INSERTED, PROTECTED and
   NAME are its settings; BLANKS the blank disks that can be made for it
   (NULL: none). */
static void drive_menu(const char *title, bool *connected, bool *inserted, bool *protected_,
                       char *name, size_t name_size, const struct blank_size *blanks)
{
	enum { R_CONNECTED, R_INSERT, R_CREATE, R_EJECT, R_PROTECT };
	int sel = 0;
	for (;;) {
		struct osd_item items[] = {
			{"Drive Connected", NULL, check(*connected), R_CONNECTED},
			{"Disk", *inserted && name[0] ? base_name(name) : "(empty)", OSD_ITEM_LABEL, -1},
			{"", NULL, OSD_ITEM_LABEL, -1},
			{"Insert a Disk Image...", NULL, OSD_ITEM_SUBMENU, R_INSERT},
			{"Create a Blank Disk...", NULL, OSD_ITEM_SUBMENU, R_CREATE},
			{"Eject", NULL, 0, R_EJECT},
			{"Write Protected", NULL, check(*protected_), R_PROTECT},
		};
		char path[FILENAME_MAX];
		int n_items = sizeof items / sizeof items[0], chosen, k;
		if (blanks == NULL) {
			/* No blank disks of this kind (optical disks). */
			for (k = 4; k < n_items - 1; k++)
				items[k] = items[k + 1];
			n_items--;
		}
		chosen = osd_menu(title, items, n_items, &sel);
		bmnext_settings_begin();
		switch (chosen) {
		case R_CREATE:
			if (!create_blank(blanks, path, sizeof path))
				break;
			snprintf(name, name_size, "%s", path);
			*inserted = true;
			*connected = true;
			*protected_ = false;
			bmnext_settings_apply(FALSE);
			break;
		case R_CONNECTED:
			*connected = !*connected;
			bmnext_settings_apply(TRUE);
			break;
		case R_INSERT:
			if (!browse("Insert a Disk Image", "disks", disk_exts, path, sizeof path))
				break;
			snprintf(name, name_size, "%s", path);
			*inserted = true;
			*connected = true;
			bmnext_settings_apply(FALSE);
			break;
		case R_EJECT:
			*inserted = false;
			bmnext_settings_apply(FALSE);
			break;
		case R_PROTECT:
			*protected_ = !*protected_;
			bmnext_settings_apply(FALSE);
			break;
		default:
			return;
		}
		if (osd_closing())
			return;
	}
}

static const char *drive_value(int connected, int inserted, const char *name)
{
	if (!connected)
		return "(none)";
	return inserted && name[0] ? base_name(name) : "(empty)";
}

static void disks_menu(void)
{
	enum { D_SCSI = 0, D_MO = 16, D_FLOPPY = 32 };
	int sel = 0;
	for (;;) {
		struct osd_item items[ESP_MAX_DEVS + MO_MAX_DRIVES + 6];
		char labels[ESP_MAX_DEVS + MO_MAX_DRIVES + 1][24];
		int n = 0, i, chosen;
		for (i = 0; i < ESP_MAX_DEVS; i++) {
			SCSIDISK *t = &ConfigureParams.SCSI.target[i];
			snprintf(labels[i], sizeof labels[i], "SCSI Disk %d", i);
			items[n++] = (struct osd_item) {
				labels[i], t->nDeviceType == SD_NONE ? "(none)"
				           : drive_value(1, t->bDiskInserted, t->szImageName),
				OSD_ITEM_SUBMENU, D_SCSI + i};
		}
		items[n++] = (struct osd_item) {"", NULL, OSD_ITEM_LABEL, -1};
		for (i = 0; i < MO_MAX_DRIVES; i++) {
			MODISK *d = &ConfigureParams.MO.drive[i];
			snprintf(labels[ESP_MAX_DEVS + i], sizeof labels[0], "Optical Drive %d", i);
			items[n++] = (struct osd_item) {
				labels[ESP_MAX_DEVS + i],
				drive_value(d->bDriveConnected, d->bDiskInserted, d->szImageName),
				OSD_ITEM_SUBMENU, D_MO + i};
		}
		items[n++] = (struct osd_item) {
			"Floppy Drive", drive_value(ConfigureParams.Floppy.drive[0].bDriveConnected,
			                            ConfigureParams.Floppy.drive[0].bDiskInserted,
			                            ConfigureParams.Floppy.drive[0].szImageName),
			OSD_ITEM_SUBMENU, D_FLOPPY};
		items[n++] = (struct osd_item) {"", NULL, OSD_ITEM_LABEL, -1};
		items[n++] = (struct osd_item) {"Disk images are in the card's disks folder.", NULL,
		                                OSD_ITEM_LABEL, -1};
		chosen = osd_menu("Disks", items, n, &sel);
		if (chosen >= D_SCSI && chosen < D_SCSI + ESP_MAX_DEVS) {
			scsi_menu(chosen - D_SCSI);
		} else if (chosen >= D_MO && chosen < D_MO + MO_MAX_DRIVES) {
			MODISK *d = &ConfigureParams.MO.drive[chosen - D_MO];
			drive_menu(labels[ESP_MAX_DEVS + chosen - D_MO], &d->bDriveConnected,
			           &d->bDiskInserted, &d->bWriteProtected, d->szImageName,
			           sizeof d->szImageName, NULL);
		} else if (chosen == D_FLOPPY) {
			FLPDISK *d = &ConfigureParams.Floppy.drive[0];
			drive_menu("Floppy Drive", &d->bDriveConnected, &d->bDiskInserted,
			           &d->bWriteProtected, d->szImageName, sizeof d->szImageName,
			           floppy_sizes);
		} else {
			return;
		}
		if (osd_closing())
			return;
	}
}

/* ---- Machine ------------------------------------------------------------------ */

/* The NeXTs Previous emulates, as its System dialog offers them. */
static const struct {
	const char *name;
	MACHINETYPE type;
	bool turbo, color;
} machines[] = {
	{"NeXT Computer (68030)", NEXT_CUBE030, false, false},
	{"NeXTcube (68040)", NEXT_CUBE040, false, false},
	{"NeXTcube Turbo", NEXT_CUBE040, true, false},
	{"NeXTstation", NEXT_STATION, false, false},
	{"NeXTstation Turbo", NEXT_STATION, true, false},
	{"NeXTstation Color", NEXT_STATION, false, true},
	{"NeXTstation Turbo Color", NEXT_STATION, true, true},
};
#define N_MACHINES ((int) (sizeof machines / sizeof machines[0]))

static int machine_index(void)
{
	int i;
	for (i = 0; i < N_MACHINES; i++)
		if (machines[i].type == ConfigureParams.System.nMachineType
		    && machines[i].turbo == ConfigureParams.System.bTurbo
		    && machines[i].color == ConfigureParams.System.bColor)
			return i;
	return 0;
}

const char *bmnext_machine_text(void)
{
	return machines[machine_index()].name;
}

static const char *rom_value(void)
{
	if (ConfigureParams.System.nMachineType == NEXT_CUBE030)
		return base_name(ConfigureParams.Rom.szRom030FileName);
	if (ConfigureParams.System.bTurbo)
		return base_name(ConfigureParams.Rom.szRomTurboFileName);
	return base_name(ConfigureParams.Rom.szRom040FileName);
}

static int memory_mb(void)
{
	int i, mb = 0;
	for (i = 0; i < 4; i++)
		mb += ConfigureParams.Memory.nMemoryBankSize[i];
	return mb;
}

/* ---- Boot -------------------------------------------------------------- */

/* What the NeXT starts from, and its power-on tests: Previous's own Boot
   dialog's options (dlgBoot.c), which it puts in the NeXT's NVRAM; a
   change restarts the NeXT (change.c). */
static void boot_menu(void)
{
	enum { B_DEVICE, B_DRAM, B_POT, B_SOUND, B_SCSI, B_LOOP, B_VERBOSE, B_EXTENDED,
	       B_VISIBLE };
	static const char *const devices[] = {"ROM Monitor", "SCSI Disk", "Network",
	                                      "Optical Disk", "Floppy", NULL};
	int sel = 0;
	for (;;) {
		CNF_BOOT *b = &ConfigureParams.Boot;
		struct osd_item items[] = {
			{"Start From", devices[b->nBootDevice % 5], OSD_ITEM_SUBMENU, B_DEVICE},
			{"", NULL, OSD_ITEM_LABEL, -1},
			{"Memory Test", NULL, check(b->bEnableDRAMTest), B_DRAM},
			{"Power-on Test", NULL, check(b->bEnablePot), B_POT},
			{"  Sound Out Test", NULL, check(b->bEnableSoundTest), B_SOUND},
			{"  SCSI Tests", NULL, check(b->bEnableSCSITest), B_SCSI},
			{"  Loop Until a Key Is Pressed", NULL, check(b->bLoopPot), B_LOOP},
			{"  Verbose", NULL, check(b->bVerbose), B_VERBOSE},
			{"Extended Diagnostics", NULL, check(b->bExtendedPot), B_EXTENDED},
			{"Show the Display During Tests", NULL, check(b->bVisible), B_VISIBLE},
			{"", NULL, OSD_ITEM_LABEL, -1},
			{"Changes here restart the NeXT. Its ROM monitor (NeXT>)", NULL, OSD_ITEM_LABEL, -1},
			{"has more: 'p' sets them, 'b' starts from a device.", NULL, OSD_ITEM_LABEL, -1},
		};
		bool *flag = NULL;
		int chosen = osd_menu("Boot", items, sizeof items / sizeof items[0], &sel), i;
		bmnext_settings_begin();
		switch (chosen) {
		case B_DEVICE:
			i = osd_choose("Start From", devices, b->nBootDevice % 5);
			if (i >= 0) {
				b->nBootDevice = (BOOT_DEVICE) i;
				bmnext_settings_apply(FALSE);
			}
			break;
		case B_DRAM: flag = &b->bEnableDRAMTest; break;
		case B_POT: flag = &b->bEnablePot; break;
		case B_SOUND: flag = &b->bEnableSoundTest; break;
		case B_SCSI: flag = &b->bEnableSCSITest; break;
		case B_LOOP: flag = &b->bLoopPot; break;
		case B_VERBOSE: flag = &b->bVerbose; break;
		case B_EXTENDED: flag = &b->bExtendedPot; break;
		case B_VISIBLE: flag = &b->bVisible; break;
		default:
			return;
		}
		if (flag) {
			*flag = !*flag;
			bmnext_settings_apply(FALSE);
		}
		if (osd_closing())
			return;
	}
}

/* Previous's mouse speed (fLinScale, keymap.c). */
static const char *const mouse_speeds[] = {"Slowest", "Slow", "Normal", "Fast", "Fastest", NULL};
static const float mouse_scales[] = {0.5f, 0.75f, 1.0f, 1.5f, 2.0f};

static int mouse_speed(void)
{
	int i, best = 2;
	for (i = 0; i < 5; i++)
		if (fabsf(mouse_scales[i] - ConfigureParams.Mouse.fLinScale)
		    < fabsf(mouse_scales[best] - ConfigureParams.Mouse.fLinScale))
			best = i;
	return best;
}

static void machine_menu(void)
{
	enum { S_MACHINE, S_ROM, S_RAM, S_BOOT, S_ETHERNET, S_REALTIME, S_SWAP, S_MOUSE };
	int sel = 0;
	for (;;) {
		char ram[16];
		struct osd_item items[] = {
			{"Machine", machines[machine_index()].name, OSD_ITEM_SUBMENU, S_MACHINE},
			{"ROM", rom_value(), OSD_ITEM_SUBMENU, S_ROM},
			{"Memory", (snprintf(ram, sizeof ram, "%d MB", memory_mb()), ram),
			 OSD_ITEM_SUBMENU, S_RAM},
			{"Boot", NULL, OSD_ITEM_SUBMENU, S_BOOT},
			{"Ethernet Connected (the NeXT's network)", NULL,
			 check(ConfigureParams.Ethernet.bEthernetConnected), S_ETHERNET},
			{"Variable Speed (real time clock)", NULL,
			 check(ConfigureParams.System.bRealtime), S_REALTIME},
			{"Swap Command and Alt", NULL, check(ConfigureParams.Keyboard.bSwapCmdAlt),
			 S_SWAP},
			{"Mouse Speed", mouse_speeds[mouse_speed()], OSD_ITEM_SUBMENU, S_MOUSE},
			{"", NULL, OSD_ITEM_LABEL, -1},
			{"Machine, ROM and Memory restart the NeXT. The 68040s", NULL, OSD_ITEM_LABEL, -1},
			{"need ROM 2.x, the Turbos ROM 3.x (in the roms folder).", NULL, OSD_ITEM_LABEL, -1},
		};
		const char *names[N_MACHINES + 1];
		char path[FILENAME_MAX];
		int chosen, i;
		chosen = osd_menu("Machine", items, sizeof items / sizeof items[0], &sel);
		bmnext_settings_begin();
		switch (chosen) {
		case S_MACHINE:
			for (i = 0; i < N_MACHINES; i++)
				names[i] = machines[i].name;
			names[N_MACHINES] = NULL;
			i = osd_choose("Machine", names, machine_index());
			if (i >= 0 && i != machine_index()) {
				ConfigureParams.System.nMachineType = machines[i].type;
				ConfigureParams.System.bTurbo = machines[i].turbo;
				ConfigureParams.System.bColor = machines[i].color;
				Configuration_SetSystemDefaults();
				bmnext_find_roms();
				bmnext_settings_apply(TRUE);
			}
			break;
		case S_ROM: {
			char *rom = ConfigureParams.System.nMachineType == NEXT_CUBE030
			            ? ConfigureParams.Rom.szRom030FileName
			            : ConfigureParams.System.bTurbo ? ConfigureParams.Rom.szRomTurboFileName
			                                            : ConfigureParams.Rom.szRom040FileName;
			static const char *const rom_exts[] = {"bin", "rom", NULL};
			if (!browse("Choose the ROM", "roms", rom_exts, path, sizeof path))
				break;
			snprintf(rom, FILENAME_MAX, "%s", path);
			bmnext_settings_apply(TRUE);
			break;
		}
		case S_RAM: {
			static const char *const sizes[] = {"8 MB", "16 MB", "32 MB", "64 MB", "128 MB", NULL};
			static const int banks[][4] = {
				{4, 4, 0, 0}, {4, 4, 4, 4}, {16, 16, 0, 0}, {16, 16, 16, 16}, {32, 32, 32, 32}
			};
			int mb = memory_mb(), current = mb >= 128 ? 4 : mb >= 64 ? 3 : mb >= 32 ? 2
			                               : mb >= 16 ? 1 : 0;
			i = osd_choose("Memory", sizes, current);
			if (i >= 0) {
				memcpy(ConfigureParams.Memory.nMemoryBankSize, banks[i], sizeof banks[i]);
				bmnext_settings_apply(TRUE);
			}
			break;
		}
		case S_REALTIME:
			ConfigureParams.System.bRealtime = !ConfigureParams.System.bRealtime;
			bmnext_settings_apply(FALSE);
			break;
		case S_SWAP:
			ConfigureParams.Keyboard.bSwapCmdAlt = !ConfigureParams.Keyboard.bSwapCmdAlt;
			bmnext_settings_apply(FALSE);
			break;
		case S_BOOT:
			boot_menu();
			break;
		case S_ETHERNET:
			/* The NeXT on a network of its own, through SLiRP: it reaches
			   what the Pi does (Network, below in the main menu). */
			ConfigureParams.Ethernet.bEthernetConnected =
				!ConfigureParams.Ethernet.bEthernetConnected;
			bmnext_settings_apply(FALSE);
			break;
		case S_MOUSE:
			i = osd_choose("Mouse Speed", mouse_speeds, mouse_speed());
			if (i >= 0) {
				ConfigureParams.Mouse.fLinScale = mouse_scales[i];
				bmnext_settings_apply(FALSE);
			}
			break;
		default:
			return;
		}
		if (osd_closing())
			return;
	}
}

/* ---- Video -------------------------------------------------------------- */

/* The picture's height as a percentage of the screen's (the scale option):
   less for a TV that cuts off the edges (overscan). From BMCoco. */
static int screen_size(void)
{
	double scale = host_option_double("scale", 1.0);
	if (scale <= 0.1 || scale > 1.0)
		scale = 1.0;
	return (int) (scale * 100.0 + 0.5);
}

static void choose_screen_size(void)
{
	static const char *const choices[] = {
		"100%: fill the screen", "95%", "90%", "85%", "80%", NULL
	};
	static const int sizes[] = {100, 95, 90, 85, 80};
	int size = screen_size(), current = 0, k, i;
	char text[16];
	for (k = 1; k < 5; k++) {
		int d = sizes[k] - size, best = sizes[current] - size;
		if ((d < 0 ? -d : d) < (best < 0 ? -best : best))
			current = k;
	}
	i = osd_choose("Screen Size", choices, current);
	if (i < 0)
		return;
	snprintf(text, sizeof text, "%d.%02d", sizes[i] / 100, sizes[i] % 100);
	save_setting("scale", text);
	bmnext_video_settings_changed();
}

static void video_menu(void)
{
	enum { V_SIZE, V_RESOLUTION, V_SHADER, V_CURVATURE, V_SMOOTH,
	       V_ACTIVITY };
	/* The Pi 5 only: its firmware ignores hdmi_mode (from BMA800XL). */
	static const char *const resolutions[] = {"auto", "1920x1080", "1280x720", NULL};
	static const char *const resolution_names[] = {"Auto", "1920x1080", "1280x720", NULL};
	int sel = 0;
	for (;;) {
		const char *res = host_option("resolution", "auto");
		int res_index = 0, n_items, k;
		char size_text[8];
		snprintf(size_text, sizeof size_text, "%d%%", screen_size());
		for (k = 0; resolutions[k] != NULL; k++)
			if (strcmp(res, resolutions[k]) == 0)
				res_index = k;
		struct osd_item items[] = {
			{"Screen Size", size_text, OSD_ITEM_SUBMENU, V_SIZE},
			{"Resolution", resolution_names[res_index], OSD_ITEM_SUBMENU, V_RESOLUTION},
			{"CRT Effect (reboot)", NULL, check(setting_on("crt_shader", FALSE)), V_SHADER},
			{"CRT Curvature (reboot)", NULL, check(setting_on("crt_curvature", FALSE)), V_CURVATURE},
			{"Smooth Scaling (reboot)", NULL, check(setting_on("smooth", FALSE)), V_SMOOTH},
			{"Show Disk Activity", NULL, check(bmnext_disk_activity()), V_ACTIVITY},
			{"", NULL, OSD_ITEM_LABEL, -1},
			{"If your TV cuts off the edges, make", NULL, OSD_ITEM_LABEL, -1},
			{"the Screen Size smaller.", NULL, OSD_ITEM_LABEL, -1},
		};
		int chosen, i;
		n_items = sizeof(items) / sizeof(items[0]);
		if (!host_display_resolution_choice()) {
			/* Resolution is only for the Pi 5 (with video_driver=kms). */
			for (k = 1; k < n_items - 1; k++)
				items[k] = items[k + 1];
			n_items--;
		}
		chosen = osd_menu("Video", items, n_items, &sel);
		switch (chosen) {
		case V_SIZE:
			choose_screen_size();
			break;
		case V_ACTIVITY:
			save_setting("disk_activity", bmnext_disk_activity() ? "0" : "1");
			bmnext_set_disk_activity(!bmnext_disk_activity());
			break;
		case V_RESOLUTION:
			i = osd_choose("Resolution", resolution_names, res_index);
			if (i >= 0 && i != res_index) {
				save_setting("resolution", resolutions[i]);
				bmnext_display_mode_changed();
			}
			break;
		case V_CURVATURE:
			save_setting("crt_curvature", setting_on("crt_curvature", FALSE) ? "0" : "1");
			reboot_needed = TRUE;
			break;
		case V_SHADER:
			save_setting("crt_shader", setting_on("crt_shader", FALSE) ? "0" : "1");
			reboot_needed = TRUE;
			break;
		case V_SMOOTH:
			save_setting("smooth", setting_on("smooth", FALSE) ? "0" : "1");
			reboot_needed = TRUE;
			break;
		default:
			return;
		}
	}
}

/* ---- Sound ---------------------------------------------------------------- */

static void sound_menu(void)
{
	enum { S_VOLUME, S_OUTPUT, S_TONE, S_TONE_NEXT };
	static const char *const volumes[] = {
		"100%", "90%", "80%", "70%", "60%", "50%", "40%", "30%", "20%", "10%", "Off", NULL
	};
	static const char *const outputs[] = {"auto", "hdmi", "analog"};
	static const char *const output_names[] = {"Automatic", "HDMI", "Headphone jack", NULL};
	int sel = 0;
	for (;;) {
		const char *out = host_option("audio_out", "auto");
		int out_index = strcmp(out, "hdmi") == 0 ? 1
		                : strcmp(out, "analog") == 0 || strcmp(out, "headphones") == 0 ? 2 : 0;
		char volume[8];
		struct osd_item items[] = {
			{"Volume", volume, OSD_ITEM_SUBMENU, S_VOLUME},
			{"Output (reboot)", output_names[out_index], OSD_ITEM_SUBMENU, S_OUTPUT},
			{"Test Tone (440 Hz)", NULL, 0, S_TONE},
			{"Test Tone (NeXT's Way)", NULL, 0, S_TONE_NEXT},
		};
		int chosen, i;
		if (bmnext_volume() == 0)
			snprintf(volume, sizeof volume, "Off");
		else
			snprintf(volume, sizeof volume, "%d%%", bmnext_volume());
		chosen = osd_menu("Sound", items, sizeof(items) / sizeof(items[0]), &sel);
		if (chosen == S_VOLUME) {
			i = osd_choose("Volume", volumes, (100 - bmnext_volume() + 5) / 10);
			if (i >= 0)
				bmnext_set_volume(100 - i * 10);
			continue;
		}
		if (chosen == S_TONE || chosen == S_TONE_NEXT) {
			bmnext_test_tone(chosen == S_TONE_NEXT);
			continue;
		}
		if (chosen != S_OUTPUT)
			return;
		i = osd_choose("Sound Output", output_names, out_index);
		if (i >= 0 && i != out_index) {
			save_setting("audio_out", outputs[i]);
			reboot_needed = TRUE;
		}
	}
}

/* ---- Network --------------------------------------------------------------- */

static char wifi_ssid[33], wifi_psk[64], wifi_country[3];

static void wpa_path(char *path, size_t size)
{
	const char *root = bmnext_sd_root();
	size_t len = strlen(root);
	snprintf(path, size, "%s%s%s", root, len > 0 && root[len - 1] == '/' ? "" : "/", WPA_CONFIG);
}

static void copy_quoted(char *dst, size_t size, const char *src)
{
	const char *end;
	if (*src == '"')
		src++;
	end = strchr(src, '"');
	if (end == NULL)
		end = src + strcspn(src, "\r\n");
	if ((size_t) (end - src) >= size)
		end = src + size - 1;
	memcpy(dst, src, end - src);
	dst[end - src] = '\0';
}

static void read_wifi_config(void)
{
	char path[256], line[160];
	FILE *f;
	wifi_ssid[0] = wifi_psk[0] = '\0';
	strcpy(wifi_country, "US");
	wpa_path(path, sizeof(path));
	f = fopen(path, "r");
	if (f == NULL)
		return;
	while (fgets(line, sizeof(line), f) != NULL) {
		char *p = line + strspn(line, " \t");
		if (strncmp(p, "country=", 8) == 0)
			copy_quoted(wifi_country, sizeof(wifi_country), p + 8);
		else if (strncmp(p, "ssid=", 5) == 0 && wifi_ssid[0] == '\0')
			copy_quoted(wifi_ssid, sizeof(wifi_ssid), p + 5);
		else if (strncmp(p, "psk=", 4) == 0 && wifi_psk[0] == '\0')
			copy_quoted(wifi_psk, sizeof(wifi_psk), p + 4);
	}
	fclose(f);
}

static int write_wifi_config(void)
{
	char path[256];
	FILE *f;
	wpa_path(path, sizeof(path));
	f = fopen(path, "w");
	if (f == NULL)
		return FALSE;
	fprintf(f, "country=%s\n\nnetwork={\n\tssid=\"%s\"\n", wifi_country, wifi_ssid);
	if (wifi_psk[0] != '\0')
		fprintf(f, "\tpsk=\"%s\"\n\tkey_mgmt=WPA-PSK\n", wifi_psk);
	else
		fprintf(f, "\tkey_mgmt=NONE\n");
	fprintf(f, "}\n");
	return fclose(f) == 0;
}

static void edit_wifi(const char *title, char *value, size_t size, int secret)
{
	char text[64];
	snprintf(text, sizeof(text), "%s", value);
	if (osd_edit(title, text, size < sizeof(text) ? size : sizeof(text), secret)) {
		snprintf(value, size, "%s", text);
		if (write_wifi_config())
			reboot_needed = TRUE;
		else
			osd_message("Cannot write " WPA_CONFIG, TRUE);
	}
}

static int compare_signal(const void *a, const void *b)
{
	return ((const struct host_wifi_ap *) b)->signal - ((const struct host_wifi_ap *) a)->signal;
}

/* Returns 1 if a network was chosen, 2 if it also needs a password. */
static int scan_wifi(void)
{
	enum { MAX_APS = 20 };
	static struct host_wifi_ap aps[MAX_APS];
	static char signal[MAX_APS][16];
	struct osd_item items[MAX_APS];
	int n, i, sel = 0, chosen;

	osd_message("Scanning for Wi-Fi networks...", FALSE);
	n = host_wifi_scan(aps, MAX_APS);
	if (n < 0) {
		if (osd_confirm("Wi-Fi is Off", "Switch to Wi-Fi and reboot?")) {
			save_setting("network", "wifi");
			power(0);
		}
		return 0;
	}
	if (n == 0) {
		osd_message("No Wi-Fi networks found.", TRUE);
		return 0;
	}
	qsort(aps, n, sizeof(aps[0]), compare_signal);
	for (i = 0; i < n; i++) {
		snprintf(signal[i], sizeof(signal[i]), "%s%d", aps[i].secure ? "* " : "", aps[i].signal);
		items[i] = (struct osd_item){aps[i].ssid, signal[i], 0, i};
	}
	chosen = osd_menu("Choose a Wi-Fi Network", items, n, &sel);
	if (chosen < 0)
		return 0;
	if (!aps[chosen].secure || strcmp(wifi_ssid, aps[chosen].ssid) != 0)
		wifi_psk[0] = '\0';
	snprintf(wifi_ssid, sizeof(wifi_ssid), "%s", aps[chosen].ssid);
	if (!write_wifi_config()) {
		osd_message("Cannot write " WPA_CONFIG, TRUE);
		return 0;
	}
	save_setting("network", "wifi");
	reboot_needed = TRUE;
	return aps[chosen].secure && wifi_psk[0] == '\0' ? 2 : 1;
}

static void network_menu(void)
{
	enum { N_DEVICE, N_SCAN, N_SSID, N_PASSWORD, N_COUNTRY, N_WEBUI, N_PIN, N_TIMEZONE, N_REBOOT };
	static const char *const devices[] = {"off", "ethernet", "wifi", NULL};
	static const char *const device_names[] = {"Off", "Ethernet", "Wi-Fi", NULL};
	int sel = 2;
	for (;;) {
		char status[64], tz[16];
		const char *net = host_option("network", "off");
		const char *pin = host_option("webui_pin", "");
		int device = strcmp(net, "ethernet") == 0 ? 1 : strcmp(net, "wifi") == 0 ? 2 : 0;
		int minutes = host_option_int("timezone", 0);
		int chosen, i;

		read_wifi_config();
		host_network_status(status, sizeof(status));
		if (minutes == 0)
			strcpy(tz, "UTC");
		else
			snprintf(tz, sizeof(tz), "UTC%+d", minutes / 60);
		{
			struct osd_item items[] = {
				{status, NULL, OSD_ITEM_LABEL, -1},
				{"", NULL, OSD_ITEM_LABEL, -1},
				{"Network", device_names[device], OSD_ITEM_SUBMENU, N_DEVICE},
				{"Scan for Wi-Fi Networks", NULL, OSD_ITEM_SUBMENU, N_SCAN},
				{"Wi-Fi Network", wifi_ssid[0] ? wifi_ssid : "(none)", OSD_ITEM_SUBMENU, N_SSID},
				{"Wi-Fi Password", wifi_psk[0] ? "********" : "(none)", OSD_ITEM_SUBMENU, N_PASSWORD},
				{"Wi-Fi Country", wifi_country, OSD_ITEM_SUBMENU, N_COUNTRY},
				{"Web UI", NULL, check(setting_on("webui", TRUE)), N_WEBUI},
				{"Web UI PIN", pin[0] ? pin : "(none)", OSD_ITEM_SUBMENU, N_PIN},
				{"Time Zone", tz, OSD_ITEM_SUBMENU, N_TIMEZONE},
				{"", NULL, OSD_ITEM_LABEL, -1},
				{"Changes apply after a reboot.", NULL, OSD_ITEM_LABEL, -1},
				{"Reboot Now", NULL, 0, N_REBOOT},
			};
			chosen = osd_menu("Network", items, sizeof(items) / sizeof(items[0]), &sel);
		}
		switch (chosen) {
		case N_DEVICE:
			i = osd_choose("Network", device_names, device);
			if (i >= 0 && i != device) {
				save_setting("network", devices[i]);
				reboot_needed = TRUE;
			}
			break;
		case N_SCAN:
			if (scan_wifi() == 2) {
				wifi_psk[0] = '\0';
				edit_wifi("Wi-Fi Password", wifi_psk, sizeof(wifi_psk), TRUE);
			}
			break;
		case N_SSID:
			edit_wifi("Wi-Fi Network Name", wifi_ssid, sizeof(wifi_ssid), FALSE);
			break;
		case N_PASSWORD:
			wifi_psk[0] = '\0';
			edit_wifi("Wi-Fi Password (blank: none)", wifi_psk, sizeof(wifi_psk), TRUE);
			break;
		case N_COUNTRY:
			edit_wifi("Two Letter Country Code", wifi_country, sizeof(wifi_country), FALSE);
			break;
		case N_WEBUI:
			save_setting("webui", setting_on("webui", TRUE) ? "0" : "1");
			reboot_needed = TRUE;
			break;
		case N_PIN: {
			char text[9];
			snprintf(text, sizeof(text), "%s", pin);
			if (osd_edit("Web UI PIN (blank: none)", text, sizeof(text), FALSE)) {
				save_setting("webui_pin", text);
				reboot_needed = TRUE;
			}
			break;
		}
		case N_TIMEZONE: {
			static char labels[27][8];
			static const char *zones[28];
			int h, current = 12;
			for (h = -12; h <= 14; h++) {
				if (h == 0)
					strcpy(labels[h + 12], "UTC");
				else
					snprintf(labels[h + 12], sizeof(labels[0]), "UTC%+d", h);
				zones[h + 12] = labels[h + 12];
				if (h * 60 == minutes)
					current = h + 12;
			}
			zones[27] = NULL;
			h = osd_choose("Time Zone", zones, current);
			if (h >= 0 && h != current) {
				save_setting_int("timezone", (h - 12) * 60);
				reboot_needed = TRUE;
			}
			break;
		}
		case N_REBOOT:
			power(0);
			return;
		default:
			return;
		}
	}
}

/* ---- Information ----------------------------------------------------------- */

static void about(void)
{
	char text[1800], status[64];
	int w, h;
	host_display_size(&w, &h);
	host_network_status(status, sizeof(status));
	snprintf(text, sizeof(text),
	         "BMNeXT " BMNEXT_VERSION "\n"
	         "Bare metal NeXT Computer, NeXTcube and NeXTstation\n"
	         "emulator for the Raspberry Pi 4 and 5.\n\n"
	         "Emulation: " PROG_NAME " (GPL 2 or later), by Andreas\n"
	         "Grabher, Simon Schubiger and Gilles Fetis, from Hatari.\n"
	         "ROM and NeXTSTEP: your own.\n"
	         "Based on BMAtariST, BMApple2, BMCoco, BMA800XL and BMC64.\n\n"
	         "Machine: %s\n"
	         "ROM: %s\n"
	         "Speed: %d%% of the real machine's\n"
	         "Display: %dx%d %.2f Hz\n"
	         "Network: %s",
	         bmnext_machine_text(), rom_value(), bmnext_speed_percent(), w, h,
	         bmnext_refresh_rate(), status);
	osd_info("About BMNeXT", text);
}

static void offer_reboot(void)
{
	if (osd_confirm("Reboot?", "Settings changed that need a reboot. Reboot now?"))
		power(0);
}

/* ---- Main menu ---------------------------------------------------------------- */

void bmnext_menu(void)
{
	enum { M_DISKS, M_RESTART, M_MACHINE, M_VIDEO, M_SOUND, M_NETWORK, M_ABOUT,
	       M_REBOOT, M_OFF };
	static int sel = 0;
	char title[48], speed[48];

	snprintf(title, sizeof(title), "BMNeXT %s", BMNEXT_VERSION);
	reboot_needed = FALSE;
	osd_open();
	for (;;) {
		struct osd_item items[] = {
			{"Disks", NULL, OSD_ITEM_SUBMENU, M_DISKS},
			{"Restart (switch the NeXT off and on)", NULL, 0, M_RESTART},
			{"", NULL, OSD_ITEM_LABEL, -1},
			{"Machine", bmnext_machine_text(), OSD_ITEM_SUBMENU, M_MACHINE},
			{"Video", NULL, OSD_ITEM_SUBMENU, M_VIDEO},
			{"Sound", NULL, OSD_ITEM_SUBMENU, M_SOUND},
			{"Network", NULL, OSD_ITEM_SUBMENU, M_NETWORK},
			{"About", NULL, OSD_ITEM_SUBMENU, M_ABOUT},
			{"", NULL, OSD_ITEM_LABEL, -1},
			{"Reboot", NULL, 0, M_REBOOT},
			{"Shut Down", NULL, 0, M_OFF},
			{"", NULL, OSD_ITEM_LABEL, -1},
			{speed, NULL, OSD_ITEM_LABEL, -1},
		};
		int chosen, leave = FALSE;
		snprintf(speed, sizeof speed, "Running at %d%% of the real machine's speed.",
		         bmnext_speed_percent());
		chosen = osd_menu(title, items, sizeof(items) / sizeof(items[0]), &sel);

		switch (chosen) {
		case M_DISKS: disks_menu(); break;
		case M_RESTART: bmnext_cold_reset(); leave = TRUE; break;
		case M_MACHINE: machine_menu(); break;
		case M_VIDEO: video_menu(); break;
		case M_SOUND: sound_menu(); break;
		case M_NETWORK: network_menu(); break;
		case M_ABOUT: about(); break;
		case M_REBOOT:
			if (osd_confirm("Reboot", "Reboot BMNeXT?"))
				power(0);
			break;
		case M_OFF:
			if (osd_confirm("Shut Down", "Shut down? It's then safe to switch off."))
				power(1);
			break;
		default:
			leave = TRUE;
			break;
		}
		if (leave || osd_closing() || bmnext_pending_requests())
			break;
	}
	if (reboot_needed && !osd_closing())
		offer_reboot();
	osd_close();
}
