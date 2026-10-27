/*
 * gui_event.c - the keyboard, mouse and gamepads into Previous, for BMNeXT.
 *
 * Replaces Previous's src/gui-sdl/sdlevent.c (SDL's event loop). Previous's
 * CPU core calls GuiEvent_EventHandler() 200 times a second of the NeXT's
 * time (Main_EventHandler(), main_next.c); the keys pressed and the mouse's
 * moves since go to the NeXT from here (keymap.c), and the platform layer
 * then shows the display when it's time (bmnext_tick()).
 *
 * F12, the power key, a gamepad's Home button or Select and Start together
 * aren't the NeXT's: they ask for the menu (or to switch off).
 *
 * This file is part of BMNeXT, a bare metal NeXT emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#include "main.h"
#include "configuration.h"
#include "event.h"

#include "../host.h"
#include "bmnext.h"

static int menu_requested;
static int pad_last_menu[HOST_PADS];

int bmnext_menu_requested(void)
{
	int r = menu_requested;
	menu_requested = FALSE;
	return r;
}

/* Keys waiting to go to the NeXT. Its keyboard (kms.c, or adb.c) holds one
   event until the NeXT reads it, and the next one replaces it: two given
   in the same 5 ms (a key let go just as the next is pressed, when typing
   fast) would lose the first, and a key would stay down. So the NeXT gets
   one each time (200 a second), and the rest wait here. */
#define KEY_QUEUE 64

static struct {
	uint8_t usage, pressed;
} key_queue[KEY_QUEUE];
static unsigned key_head, key_tail;

void bmnext_queue_key(int usage, int pressed)
{
	if (key_tail - key_head < KEY_QUEUE) {
		key_queue[key_tail % KEY_QUEUE].usage = (uint8_t) usage;
		key_queue[key_tail % KEY_QUEUE].pressed = pressed != 0;
		key_tail++;
	}
}

static void keyboard(void)
{
	int usage, pressed;
	while (host_key_event(&usage, &pressed)) {
		/* The power key: Shut Down (the Pi 400 sends it for Fn+F10). */
		if (usage == USAGE_POWER) {
			if (pressed)
				bmnext_request_power(1);
			continue;
		}
		if (usage == KEY_MENU) {
			if (pressed)
				menu_requested = TRUE;
			continue;
		}
		bmnext_queue_key(usage, pressed);
	}
	if (key_head != key_tail) {
		bmnext_key(key_queue[key_head % KEY_QUEUE].usage,
		           key_queue[key_head % KEY_QUEUE].pressed);
		key_head++;
	}
}

/* Forgets the keys waiting (bmnext_release_keys()). */
void bmnext_drop_queued_keys(void)
{
	key_head = key_tail;
}

static void mouse(void)
{
	int dx, dy, buttons;
	if (host_mouse_read(&dx, &dy, &buttons))
		bmnext_mouse(dx, dy, buttons);
}

static void pads(void)
{
	int pad;
	for (pad = 0; pad < HOST_PADS; pad++) {
		int state = host_pad_state(pad);
		int menu;
		if (state < 0)
			state = 0;
		menu = (state & HOST_PAD_HOME)
		       || ((state & HOST_PAD_SELECT) && (state & HOST_PAD_START));
		if (menu && !pad_last_menu[pad])
			menu_requested = TRUE;
		pad_last_menu[pad] = menu;
	}
}

/* ---- Previous's event functions ----------------------------------------------- */

void GuiEvent_EventHandler(void)
{
	keyboard();
	mouse();
	pads();
	bmnext_tick();
}

void GuiEvent_ResetKeys(void)
{
	bmnext_release_keys();
}

void GuiEvent_WarpMouse(void)
{
}

void GuiEvent_InitEventQueue(void)
{
}

void GuiEvent_InitSpecialEvent(void)
{
}

void GuiEvent_SendSpecialEvent(int type)
{
}

void GuiEvent_EventQueueHandler(void)
{
}

void UI_Init(void)
{
}

void UI_UnInit(void)
{
}

void UI_Quit(void)
{
}
