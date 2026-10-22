/*
 * nextbus.c - the NeXTbus, without the NeXTdimension, for BMNeXT.
 *
 * Replaces Previous's NextBus.cpp (C++), which also puts NeXTdimension
 * boards in the Cube's slots. The NeXTdimension is a board of its own,
 * with an Intel i860 and 1120x832 32-bit colour, and Previous runs its
 * i860 on a thread of its own (src/dimension/): BMNeXT leaves it out. Here
 * the CPU board (slot 0) answers as in NextBus.cpp, and every other slot
 * is empty, so the NeXT gets a bus error there, as on a real Cube with
 * nothing in it. Previous's settings may still ask for a board
 * (ConfigureParams.Dimension): it is ignored.
 *
 * The NeXTdimension functions the rest of Previous calls (dimension.hpp)
 * are here too, doing nothing.
 *
 * This file is part of BMNeXT, a bare metal NeXT emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#include "main.h"
#include "configuration.h"
#include "log.h"
#include "m68000.h"
#include "nbic.h"
#include "NextBus.hpp"
#include "dimension.hpp"

#define SLOT(addr)  (((addr) & 0x0F000000) >> 24)
#define BOARD(addr) (((addr) & 0xF0000000) >> 28)

static uint32_t bus_error(uint32_t addr, int read, int size, uint32_t val, const char *acc)
{
	Log_Printf(LOG_WARN, "[NextBus] Bus error %s at %08X", acc, addr);
	M68000_BusError(addr, read, size, BUS_ERROR_ACCESS_DATA, val);
	return 0;
}

/* ---- Slot memory: the CPU board's, in slot 0 -------------------------------- */

uae_u32 nextbus_slot_lget(uaecptr addr)
{
	if (SLOT(addr) == 0)
		return nb_cpu_slot_lget(addr);
	return bus_error(addr, BUS_ERROR_READ, BUS_ERROR_SIZE_LONG, 0, "lget");
}

uae_u32 nextbus_slot_wget(uaecptr addr)
{
	if (SLOT(addr) == 0)
		return nb_cpu_slot_wget(addr);
	return bus_error(addr, BUS_ERROR_READ, BUS_ERROR_SIZE_WORD, 0, "wget");
}

uae_u32 nextbus_slot_bget(uaecptr addr)
{
	if (SLOT(addr) == 0)
		return nb_cpu_slot_bget(addr);
	return bus_error(addr, BUS_ERROR_READ, BUS_ERROR_SIZE_BYTE, 0, "bget");
}

void nextbus_slot_lput(uaecptr addr, uae_u32 val)
{
	if (SLOT(addr) == 0)
		nb_cpu_slot_lput(addr, val);
	else
		bus_error(addr, BUS_ERROR_WRITE, BUS_ERROR_SIZE_LONG, val, "lput");
}

void nextbus_slot_wput(uaecptr addr, uae_u32 val)
{
	if (SLOT(addr) == 0)
		nb_cpu_slot_wput(addr, (uint16_t) val);
	else
		bus_error(addr, BUS_ERROR_WRITE, BUS_ERROR_SIZE_WORD, val, "wput");
}

void nextbus_slot_bput(uaecptr addr, uae_u32 val)
{
	if (SLOT(addr) == 0)
		nb_cpu_slot_bput(addr, (uint8_t) val);
	else
		bus_error(addr, BUS_ERROR_WRITE, BUS_ERROR_SIZE_BYTE, val, "bput");
}

/* ---- Board memory: no boards ---------------------------------------------- */

uae_u32 nextbus_board_lget(uaecptr addr)
{
	return bus_error(addr, BUS_ERROR_READ, BUS_ERROR_SIZE_LONG, 0, "lget");
}

uae_u32 nextbus_board_wget(uaecptr addr)
{
	return bus_error(addr, BUS_ERROR_READ, BUS_ERROR_SIZE_WORD, 0, "wget");
}

uae_u32 nextbus_board_bget(uaecptr addr)
{
	return bus_error(addr, BUS_ERROR_READ, BUS_ERROR_SIZE_BYTE, 0, "bget");
}

void nextbus_board_lput(uaecptr addr, uae_u32 val)
{
	bus_error(addr, BUS_ERROR_WRITE, BUS_ERROR_SIZE_LONG, val, "lput");
}

void nextbus_board_wput(uaecptr addr, uae_u32 val)
{
	bus_error(addr, BUS_ERROR_WRITE, BUS_ERROR_SIZE_WORD, val, "wput");
}

void nextbus_board_bput(uaecptr addr, uae_u32 val)
{
	bus_error(addr, BUS_ERROR_WRITE, BUS_ERROR_SIZE_BYTE, val, "bput");
}

void nextbus_init(void)
{
	int i;
	for (i = 0; i < ND_MAX_BOARDS; i++)
		if (ConfigureParams.Dimension.board[i].bEnabled)
			printf("BMNeXT: no NeXTdimension in slot %d (not emulated)\n", ND_SLOT(i));
}

void nextbus_uninit(void)
{
}

void NextBus_Reset(void)
{
}

void NextBus_Pause(bool pause)
{
}

/* ---- The NeXTdimension (none) --------------------------------------------- */

static void i860_none(int cycles)
{
}

i860_run_func i860_Run = i860_none;

/* Its interrupts (cycInt.c) never start: nd_start_interrupts() is the
   board's. */
void ND_VBL_Handler(void)
{
}

void ND_Video_VBL_Handler(void)
{
}

void nd_start_interrupts(void)
{
}

void nd_display_repaint(void)
{
}

bool nd_video_enabled(int slot)
{
	return false;
}

uint32_t *nd_vram_for_slot(int slot)
{
	return NULL;
}

void nd_start_debugger(void)
{
}

const char *nd_reports(uint64_t realTime, uint64_t hostTime)
{
	return "";
}
