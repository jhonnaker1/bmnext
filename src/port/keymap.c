/*
 * keymap.c - USB keyboards and mice as the NeXT's, for BMNeXT.
 *
 * Replaces Previous's src/gui-sdl/sdlkeymap.c, with its "scancode" mapping
 * (SDL's scancodes are USB HID usages): each PC key is the NeXT key in the
 * same place. The NeXT's keyboard sends its modifier keys as bits with
 * every key (kms.c); the Turbo machines with an ADB keyboard (Previous's
 * ADB setting) send them as keys of their own (adb.c). As in Previous:
 *
 *   Control            the NeXT's Control
 *   Windows / Command  Command (Alt with Previous's "swap Cmd and Alt")
 *   Alt / Option       Alternate (Command with the swap)
 *   F10 or Delete      Power (the NeXT's power key: it asks to shut down)
 *   F5/F6, End/Home    sound volume down/up
 *   F1/F2, PgDn/PgUp   brightness down/up
 *
 * F12 is BMNeXT's menu.
 *
 * This file is part of BMNeXT, a bare metal NeXT emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

const char Keymap_fileid[] = "BMNeXT keymap.c";

#include "main.h"
#include "configuration.h"
#include "keymap.h"
#include "kms.h"
#include "adb.h"
#include "tablet.h"

#include "../host.h"
#include "bmnext.h"

/* USB HID usage (keyboard page) to NeXT key code (kms.h). */
static const uint8_t usage_to_next[0xe8] = {
	[0x04] = NEXTKEY_a, [0x05] = NEXTKEY_b, [0x06] = NEXTKEY_c, [0x07] = NEXTKEY_d,
	[0x08] = NEXTKEY_e, [0x09] = NEXTKEY_f, [0x0a] = NEXTKEY_g, [0x0b] = NEXTKEY_h,
	[0x0c] = NEXTKEY_i, [0x0d] = NEXTKEY_j, [0x0e] = NEXTKEY_k, [0x0f] = NEXTKEY_l,
	[0x10] = NEXTKEY_m, [0x11] = NEXTKEY_n, [0x12] = NEXTKEY_o, [0x13] = NEXTKEY_p,
	[0x14] = NEXTKEY_q, [0x15] = NEXTKEY_r, [0x16] = NEXTKEY_s, [0x17] = NEXTKEY_t,
	[0x18] = NEXTKEY_u, [0x19] = NEXTKEY_v, [0x1a] = NEXTKEY_w, [0x1b] = NEXTKEY_x,
	[0x1c] = NEXTKEY_y, [0x1d] = NEXTKEY_z,
	[0x1e] = NEXTKEY_1, [0x1f] = NEXTKEY_2, [0x20] = NEXTKEY_3, [0x21] = NEXTKEY_4,
	[0x22] = NEXTKEY_5, [0x23] = NEXTKEY_6, [0x24] = NEXTKEY_7, [0x25] = NEXTKEY_8,
	[0x26] = NEXTKEY_9, [0x27] = NEXTKEY_0,
	[0x28] = NEXTKEY_RETURN,
	[0x29] = NEXTKEY_ESC,
	[0x2a] = NEXTKEY_DELETE,        /* Backspace */
	[0x2b] = NEXTKEY_TAB,
	[0x2c] = NEXTKEY_SPACE,
	[0x2d] = NEXTKEY_MINUS,
	[0x2e] = NEXTKEY_EQUALS,
	[0x2f] = NEXTKEY_OPENBRACKET,
	[0x30] = NEXTKEY_CLOSEBRACKET,
	[0x31] = NEXTKEY_BACKSLASH,
	[0x33] = NEXTKEY_SEMICOLON,
	[0x34] = NEXTKEY_QUOTE,
	[0x35] = NEXTKEY_BACKQUOTE,
	[0x36] = NEXTKEY_COMMA,
	[0x37] = NEXTKEY_PERIOD,
	[0x38] = NEXTKEY_SLASH,
	[0x3a] = NEXTKEY_BRGHTNESS_DOWN, /* F1 */
	[0x3b] = NEXTKEY_BRIGHTNESS_UP,  /* F2 */
	[0x3e] = NEXTKEY_VOLUME_DOWN,    /* F5 */
	[0x3f] = NEXTKEY_VOLUME_UP,      /* F6 */
	[0x43] = NEXTKEY_POWER,          /* F10 */
	[0x4a] = NEXTKEY_VOLUME_UP,      /* Home */
	[0x4b] = NEXTKEY_BRIGHTNESS_UP,  /* Page Up */
	[0x4c] = NEXTKEY_POWER,          /* Delete */
	[0x4d] = NEXTKEY_VOLUME_DOWN,    /* End */
	[0x4e] = NEXTKEY_BRGHTNESS_DOWN, /* Page Down */
	[0x4f] = NEXTKEY_RIGHT_ARROW,
	[0x50] = NEXTKEY_LEFT_ARROW,
	[0x51] = NEXTKEY_DOWN_ARROW,
	[0x52] = NEXTKEY_UP_ARROW,
	[0x53] = NEXTKEY_BACKQUOTE,      /* Num Lock */
	[0x54] = NEXTKEY_KEYPAD_DIVIDE,
	[0x55] = NEXTKEY_KEYPAD_MULTIPLY,
	[0x56] = NEXTKEY_KEYPAD_MINUS,
	[0x57] = NEXTKEY_KEYPAD_PLUS,
	[0x58] = NEXTKEY_KEYPAD_ENTER,
	[0x59] = NEXTKEY_KEYPAD_1, [0x5a] = NEXTKEY_KEYPAD_2, [0x5b] = NEXTKEY_KEYPAD_3,
	[0x5c] = NEXTKEY_KEYPAD_4, [0x5d] = NEXTKEY_KEYPAD_5, [0x5e] = NEXTKEY_KEYPAD_6,
	[0x5f] = NEXTKEY_KEYPAD_7, [0x60] = NEXTKEY_KEYPAD_8, [0x61] = NEXTKEY_KEYPAD_9,
	[0x62] = NEXTKEY_KEYPAD_0,
	[0x63] = NEXTKEY_KEYPAD_PERIOD,
	[0x64] = NEXTKEY_BACKSLASH,      /* the ISO key left of Z */
	[0x67] = NEXTKEY_KEYPAD_EQUALS,
};

/* The same for an ADB keyboard (adb.h), modifier keys included. */
static const uint8_t usage_to_apple[0xe8] = {
	[0x04] = APPLEKEY_a, [0x05] = APPLEKEY_b, [0x06] = APPLEKEY_c, [0x07] = APPLEKEY_d,
	[0x08] = APPLEKEY_e, [0x09] = APPLEKEY_f, [0x0a] = APPLEKEY_g, [0x0b] = APPLEKEY_h,
	[0x0c] = APPLEKEY_i, [0x0d] = APPLEKEY_j, [0x0e] = APPLEKEY_k, [0x0f] = APPLEKEY_l,
	[0x10] = APPLEKEY_m, [0x11] = APPLEKEY_n, [0x12] = APPLEKEY_o, [0x13] = APPLEKEY_p,
	[0x14] = APPLEKEY_q, [0x15] = APPLEKEY_r, [0x16] = APPLEKEY_s, [0x17] = APPLEKEY_t,
	[0x18] = APPLEKEY_u, [0x19] = APPLEKEY_v, [0x1a] = APPLEKEY_w, [0x1b] = APPLEKEY_x,
	[0x1c] = APPLEKEY_y, [0x1d] = APPLEKEY_z,
	[0x1e] = APPLEKEY_1, [0x1f] = APPLEKEY_2, [0x20] = APPLEKEY_3, [0x21] = APPLEKEY_4,
	[0x22] = APPLEKEY_5, [0x23] = APPLEKEY_6, [0x24] = APPLEKEY_7, [0x25] = APPLEKEY_8,
	[0x26] = APPLEKEY_9, [0x27] = APPLEKEY_0,
	[0x28] = APPLEKEY_RETURN,
	[0x29] = APPLEKEY_ESC,
	[0x2a] = APPLEKEY_DELETE,
	[0x2b] = APPLEKEY_TAB,
	[0x2c] = APPLEKEY_SPACE,
	[0x2d] = APPLEKEY_MINUS,
	[0x2e] = APPLEKEY_EQUALS,
	[0x2f] = APPLEKEY_OPENBRACKET,
	[0x30] = APPLEKEY_CLOSEBRACKET,
	[0x31] = APPLEKEY_BACKSLASH,
	[0x33] = APPLEKEY_SEMICOLON,
	[0x34] = APPLEKEY_QUOTE,
	[0x35] = APPLEKEY_BACKQUOTE,
	[0x36] = APPLEKEY_COMMA,
	[0x37] = APPLEKEY_PERIOD,
	[0x38] = APPLEKEY_SLASH,
	[0x39] = APPLEKEY_CAPS_LOCK,
	[0x3a] = APPLEKEY_BRIGHTNESS_DOWN,
	[0x3b] = APPLEKEY_BRIGHTNESS_UP,
	[0x3e] = APPLEKEY_VOLUME_DOWN,
	[0x3f] = APPLEKEY_VOLUME_UP,
	[0x43] = APPLEKEY_POWER,
	[0x4a] = APPLEKEY_VOLUME_UP,
	[0x4b] = APPLEKEY_BRIGHTNESS_UP,
	[0x4c] = APPLEKEY_POWER,
	[0x4d] = APPLEKEY_VOLUME_DOWN,
	[0x4e] = APPLEKEY_BRIGHTNESS_DOWN,
	[0x4f] = APPLEKEY_RIGHT_ARROW,
	[0x50] = APPLEKEY_LEFT_ARROW,
	[0x51] = APPLEKEY_DOWN_ARROW,
	[0x52] = APPLEKEY_UP_ARROW,
	[0x53] = APPLEKEY_BACKQUOTE,
	[0x54] = APPLEKEY_KEYPAD_DIVIDE,
	[0x55] = APPLEKEY_KEYPAD_MULTIPLY,
	[0x56] = APPLEKEY_KEYPAD_MINUS,
	[0x57] = APPLEKEY_KEYPAD_PLUS,
	[0x58] = APPLEKEY_KEYPAD_ENTER,
	[0x59] = APPLEKEY_KEYPAD_1, [0x5a] = APPLEKEY_KEYPAD_2, [0x5b] = APPLEKEY_KEYPAD_3,
	[0x5c] = APPLEKEY_KEYPAD_4, [0x5d] = APPLEKEY_KEYPAD_5, [0x5e] = APPLEKEY_KEYPAD_6,
	[0x5f] = APPLEKEY_KEYPAD_7, [0x60] = APPLEKEY_KEYPAD_8, [0x61] = APPLEKEY_KEYPAD_9,
	[0x62] = APPLEKEY_KEYPAD_0,
	[0x63] = APPLEKEY_KEYPAD_PERIOD,
	[0x64] = APPLEKEY_LESS,
	[0x65] = APPLEKEY_HELP,          /* the Menu key */
	[0x67] = APPLEKEY_KEYPAD_EQUALS,
	[0xe0] = APPLEKEY_CTL_LEFT,
	[0xe1] = APPLEKEY_SHIFT_LEFT,
	[0xe4] = APPLEKEY_HELP,          /* right Control */
	[0xe5] = APPLEKEY_SHIFT_LEFT,
};

#define NO_KEY 0xff

static int adb(void)
{
	return ConfigureParams.System.bADB && ConfigureParams.System.bTurbo;
}

/* The modifier keys held (USB usages 0xe0-0xe7, bit 0 left Control) and
   Caps Lock's state. */
static unsigned held_mods;
static int caps_lock;
static uint8_t key_down[0xe8];

static uint8_t next_modifiers(void)
{
	int swap = ConfigureParams.Keyboard.bSwapCmdAlt;
	uint8_t m = 0;
	if (held_mods & 0x11)
		m |= NEXTKEY_MOD_META;                            /* Control */
	if ((held_mods & 0x02) || caps_lock)
		m |= NEXTKEY_MOD_LSHIFT;
	if (held_mods & 0x20)
		m |= NEXTKEY_MOD_RSHIFT;
	if (held_mods & 0x08)
		m |= swap ? NEXTKEY_MOD_LALT : NEXTKEY_MOD_LCTRL;  /* left Windows */
	if (held_mods & 0x80)
		m |= swap ? NEXTKEY_MOD_RALT : NEXTKEY_MOD_RCTRL;
	if (held_mods & 0x04)
		m |= swap ? NEXTKEY_MOD_LCTRL : NEXTKEY_MOD_LALT;  /* left Alt */
	if (held_mods & 0x40)
		m |= swap ? NEXTKEY_MOD_RCTRL : NEXTKEY_MOD_RALT;
	return m;
}

static uint8_t apple_key(int usage)
{
	int swap = ConfigureParams.Keyboard.bSwapCmdAlt;
	switch (usage) {
	case 0xe3: case 0xe7:
		return swap ? APPLEKEY_OPTION_LEFT : APPLEKEY_APPLE_LEFT;
	case 0xe2: case 0xe6:
		return swap ? APPLEKEY_APPLE_LEFT : APPLEKEY_OPTION_LEFT;
	default:
		return usage_to_apple[usage] ? usage_to_apple[usage] : NO_KEY;
	}
}

void bmnext_key(int usage, int pressed)
{
	if (usage < 0 || usage >= (int) sizeof usage_to_next)
		return;
	if (!pressed && !key_down[usage])
		return;
	key_down[usage] = pressed != 0;
	if (adb()) {
		uint8_t key = apple_key(usage);
		if (key == NO_KEY)
			return;
		if (pressed)
			adb_keydown(key);
		else
			adb_keyup(key);
		return;
	}
	if (usage >= 0xe0) {
		/* A modifier: the NeXT gets the new set with no key. */
		if (pressed)
			held_mods |= 1u << (usage - 0xe0);
		else
			held_mods &= ~(1u << (usage - 0xe0));
		if (pressed)
			kms_keydown(next_modifiers(), NEXTKEY_NONE);
		else
			kms_keyup(next_modifiers(), NEXTKEY_NONE);
		return;
	}
	if (usage == 0x39) {
		/* Caps Lock: the NeXT has none; Previous holds Shift while it's
		   on. */
		if (pressed) {
			caps_lock = !caps_lock;
			kms_keydown(next_modifiers(), NEXTKEY_NONE);
		}
		return;
	}
	if (usage_to_next[usage] == NEXTKEY_NONE)
		return;
	if (pressed)
		kms_keydown(next_modifiers(), usage_to_next[usage]);
	else
		kms_keyup(next_modifiers(), usage_to_next[usage]);
}

void bmnext_release_keys(void)
{
	int usage;
	for (usage = 0; usage < (int) sizeof key_down; usage++)
		if (key_down[usage])
			bmnext_key(usage, 0);
}

/* A USB mouse's counts to the NeXT's, as Previous's SDL front end scales
   them (sdlevent.c): Previous's mouse settings (fLinScale, fExpScale, in
   previous.cfg) times 0.75 for the NeXT's own mouse (the ADB mouse is 100
   counts an inch, as taken), with acceleration: the faster the move, the
   more it is scaled up (fExpScale below 1). What is lost rounding is kept
   for the next move. */
static void scale_motion(int *dx, int *dy)
{
	static float frac_x, frac_y;
	float x = (float) *dx, y = (float) *dy;
	float lin = ConfigureParams.Mouse.fLinScale * (adb() ? 1.0f : 0.75f);
	float e = ConfigureParams.Mouse.fExpScale;
	if (e != 1.0f) {
		float sum = fabsf(x) + fabsf(y);
		if (sum > 0.0f)
			lin *= powf(sum, e) / sum;
	}
	x = x * lin + frac_x;
	y = y * lin + frac_y;
	*dx = (int) x;
	*dy = (int) y;
	frac_x = x - (float) *dx;
	frac_y = y - (float) *dy;
}

/* The mouse moved (relative, in the USB mouse's counts) or a button
   changed. */
void bmnext_mouse(int dx, int dy, int buttons)
{
	static int last_buttons;
	int changed = buttons ^ last_buttons;
	int tablet = ConfigureParams.Tablet.nTabletType && bTabletEnabled;
	last_buttons = buttons;
	if (!tablet && (dx || dy))
		scale_motion(&dx, &dy);
	if (dx || dy) {
		if (tablet)
			tablet_pen_move(dx, dy, 0, 0);
		else if (adb())
			adb_mouse_move(dx, dy);
		else
			kms_mouse_move(dx, dy);
	}
	if (changed & 1) {
		if (tablet)
			tablet_pen_button(true, buttons & 1);
		else if (adb())
			adb_mouse_button(true, buttons & 1);
		else
			kms_mouse_button(true, buttons & 1);
	}
	if (changed & 2) {
		if (tablet)
			tablet_pen_button(false, (buttons & 2) != 0);
		else if (adb())
			adb_mouse_button(false, (buttons & 2) != 0);
		else
			kms_mouse_button(false, (buttons & 2) != 0);
	}
}

/* ---- Previous's keymap functions ---------------------------------------------- */

void Keymap_Init(void)
{
	held_mods = 0;
	caps_lock = 0;
	memset(key_down, 0, sizeof key_down);
}

/* Previous's own shortcuts (its F12 settings, Ctrl-Alt-...) are not used:
   BMNeXT's menu (F12) has what they do. */
void Keymap_InitShortcutDefaultKeys(void)
{
	memset(ConfigureParams.Shortcut.withoutModifier, 0,
	       sizeof ConfigureParams.Shortcut.withoutModifier);
	memset(ConfigureParams.Shortcut.withModifier, 0,
	       sizeof ConfigureParams.Shortcut.withModifier);
}

int Keymap_GetKeyFromName(const char *name)
{
	return name != NULL ? (int) strtol(name, NULL, 0) : 0;
}

const char *Keymap_GetKeyName(int keycode)
{
	static char name[8];
	if (!keycode)
		return "";
	snprintf(name, sizeof name, "0x%02x", keycode);
	return name;
}
