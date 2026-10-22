/*
 * main_next.c - Previous's main loop, for BMNeXT.
 *
 * Replaces Previous's main.c. With config.h's ENABLE_RENDERING_THREAD,
 * Previous runs its CPU on the main thread, and every 5 ms of the NeXT's
 * time its CPU core calls Main_EventHandler(), which reads the keyboard and
 * mouse (gui_event.c) and keeps the emulation to real time (timing.c's
 * Timing_Sync() sleeps when the NeXT is ahead). BMNeXT does the same, with
 * no SDL and none of main.c's threads: platform.c's bmnext_tick(), from
 * GuiEvent_EventHandler(), shows the NeXT's display and opens the menu.
 *
 * The NeXT can switch itself off (NeXTSTEP's Power Off, or the power key in
 * the ROM monitor): Previous then quits (Main_RequestQuit()); here a message
 * says so and the NeXT is switched on again when a key is pressed. The same
 * when its CPU halts.
 *
 * This file is part of BMNeXT, a bare metal NeXT emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

const char Main_fileid[] = "BMNeXT main_next.c";

#include "main.h"
#include "configuration.h"
#include "cycInt.h"
#include "debugui.h"
#include "dialog.h"
#include "dsp.h"
#include "event.h"
#include "ioMem.h"
#include "keymap.h"
#include "log.h"
#include "m68000.h"
#include "paths.h"
#include "reset.h"
#include "screen.h"
#include "snd.h"
#include "statusbar.h"
#include "timing.h"
#include "NextBus.hpp"

#include "../host.h"
#include "bmnext.h"
#include "osd.h"

volatile bool bQuitProgram = false;
volatile bool bEmulationActive = false;

static volatile bool switched_off, halted;
static char stop_text[FILENAME_MAX + 80];

/* ---- Speed ------------------------------------------------------------------ */

static uint64_t lastRT;
static uint64_t lastCycles;
static double speedFactor = 1.0;
static char speedMsg[32];

static void Main_Speed(uint64_t realTime, uint64_t hostTime)
{
	uint64_t dRT = realTime - lastRT;
	if (dRT == 0)
		return;
	speedFactor = nCyclesMainCounter - lastCycles;
	speedFactor /= ConfigureParams.System.nCpuFreq;
	speedFactor /= dRT;
	lastRT = realTime;
	lastCycles = nCyclesMainCounter;
}

void Main_SpeedReset(void)
{
	uint64_t realTime, hostTime;
	Timing_GetTimes(&realTime, &hostTime);
	lastRT = realTime;
	lastCycles = nCyclesMainCounter;
}

const char *Main_SpeedMsg(void)
{
	snprintf(speedMsg, sizeof speedMsg, "%dMHz/",
	         (int) (ConfigureParams.System.nCpuFreq * speedFactor + 0.5));
	return speedMsg;
}

/* In percent of the real machine's speed, over the last two seconds. */
int bmnext_speed_percent(void)
{
	return (int) (speedFactor * 100.0 + 0.5);
}

/* ---- Pausing ---------------------------------------------------------------- */

bool Main_PauseEmulation(bool visualize)
{
	if (!bEmulationActive)
		return false;
	bEmulationActive = false;
	Timing_Pause(true);
	Sound_Pause(true);
	NextBus_Pause(true);
	return true;
}

bool Main_UnPauseEmulation(void)
{
	if (bEmulationActive)
		return false;
	NextBus_Pause(false);
	Sound_Pause(false);
	Timing_Pause(false);
	bEmulationActive = true;
	return true;
}

/* ---- Stopping ----------------------------------------------------------------- */

/* The CPU halted (a double bus fault: the wrong ROM for the machine, for
   one). Previous asks whether to restart the CPU, which would halt again
   at once in that case; here the NeXT stops until a key is pressed, and
   then starts again. */
void Main_Halt(void)
{
	Log_Printf(LOG_WARN, "Fatal error: CPU halted!");
	halted = true;
	M68000_Stop();
}

void Main_HaltDialog(void)
{
	Main_Halt();
}

/* The NeXT switched itself off (rtcnvram.c), or Previous's debugger or a
   shortcut quit. */
void Main_RequestQuit(bool confirm)
{
	printf("BMNeXT: the NeXT switched itself off\n");
	switched_off = true;
	M68000_Stop();
}

/* Previous can't go on (it couldn't load the ROM, for one): the main loop
   shows why. */
void Main_ErrorExit(const char *msg1, const char *msg2, int errval)
{
	char text[sizeof stop_text];
	snprintf(text, sizeof text, "%s%s%s", msg1 ? msg1 : "Previous stopped",
	         msg2 ? ": " : "", msg2 ? msg2 : "");
	printf("Previous: error %d: %s\n", errval, text);
	bmnext_stop(text);
}

void bmnext_stop(const char *why)
{
	if (stop_text[0] == '\0')
		snprintf(stop_text, sizeof stop_text, "%s", why);
	printf("BMNeXT: %s\n", why);
	bQuitProgram = true;
	M68000_Stop();
}

/* ---- The main loop ---------------------------------------------------------- */

/* Called by the CPU core every 5 ms of the NeXT's time (EVENT_MAIN_EVENT). */
void Main_EventHandler(void)
{
	static int statusBarUpdate;
	if (++statusBarUpdate > 400) {
		uint64_t vt, rt;
		Timing_GetTimes(&rt, &vt);
		Main_Speed(rt, vt);
		statusBarUpdate = 0;
	}
	GuiEvent_EventHandler();
	Timing_Sync();
	CycInt_AddTimeEvent((1000 * 1000) / 200, 0, EVENT_MAIN_EVENT);
}

/* Starts Previous with the settings in CONFIG_FILE (written back there when
   they change), and runs it. Returns only if it can't go on: the reason is
   in *WHY. */
void bmnext_previous_run(const char *config_file, void (*settings)(void), const char **why)
{
	/* main.c's main(). */
	Paths_Init("/previous");
	Configuration_SetDefault();
	snprintf(sConfigFileName, sizeof sConfigFileName, "%s", config_file);
	Configuration_Load(NULL);
	/* BMNeXT's choices over the saved ones (platform.c): the ROMs found on
	   the card, no NeXTdimension. */
	if (settings)
		settings();
	Configuration_Apply(true);

	/* main.c's Main_Init(). */
	if (!Log_Init()) {
		*why = "Previous's log could not be started";
		return;
	}
	Keymap_Init();
	M68000_Init();
	DSP_Init();
	IoMem_Init();
	CycInt_Reset();
	DebugUI_Init();
	Dialog_CheckFiles();
	if (!bQuitProgram && Reset_Cold() != 0)
		bmnext_stop("The NeXT could not be started (Reset_Cold)");
	if (bQuitProgram) {
		*why = stop_text;
		return;
	}
	Configuration_Save();

	/* main.c's Main_Loop(), and Main_Thread()'s restarting the CPU. */
	Main_UnPauseEmulation();
	for (;;) {
		CycInt_AddTimeEvent(1000, 0, EVENT_MAIN_EVENT);
		M68000_Start();
		if (bQuitProgram) {
			*why = stop_text[0] ? stop_text : "Previous stopped";
			return;
		}
		if (switched_off || halted) {
			const char *text = halted
				? "The NeXT's CPU halted (is the ROM the machine's?). "
				  "Press a key to restart it, or F12 for the menu."
				: "The NeXT has switched itself off. Press a key to switch it on again.";
			switched_off = halted = false;
			Main_PauseEmulation(false);
			bmnext_wait_restart(text);
			Main_UnPauseEmulation();
			Reset_Cold();
		}
	}
}
