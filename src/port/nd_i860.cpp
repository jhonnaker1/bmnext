/*
 * nd_i860.cpp - the NeXTdimension's i860 thread for BMNeXT, which skips
 * the i860's wait loops.
 *
 * Previous runs the i860 on a thread of its own (i860_cpu_device::run(),
 * src/dimension/i860.cpp): it handles the messages from the NeXT, and runs
 * the i860 16 cycles at a time while it has cycles left (the NeXT hands out
 * 33 MHz's worth, ND_VBL_Handler()). host_next.c runs that thread on the
 * helper core (the Pi 5's core 3), and starts this one in its place, which
 * does the same and one thing more.
 *
 * The i860 spends nearly all its time waiting: the board's ROM, and then
 * NeXTSTEP's NeXTdimension kernel, spin in a loop of one or two
 * instructions until the NeXT gives them something to do. Run at full
 * speed, that takes most of a Pi 5 core for nothing. So when the i860 is
 * spinning - its program counter stays within a few instructions and none
 * of its integer registers changes - it is run a little at a time instead,
 * 16 cycles every 100 microseconds, enough to see what it waits for (a word
 * the NeXT writes, or an interrupt, a message) soon after it comes. A loop
 * that does work (fills memory, counts down) changes its registers, and
 * runs at full speed.
 *
 * The i860's program counter and registers are private in Previous's
 * class; this file, and only this file, reads them (private is made
 * public before Previous's headers, which changes nothing in the class's
 * layout).
 *
 * This file is part of BMNeXT, a bare metal NeXT emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

/* The system headers first, as they are: only Previous's classes are
   opened up. */
#include <assert.h>
#include <ctype.h>
#include <fenv.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"

#define private public
#include "dimension.hpp"
#undef private

/* A loop at most this many bytes long (eight instructions)... */
#define SPIN_BYTES 32
/* ...that has gone round this many cycles without its registers changing
   is a wait loop. */
#define SPIN_CYCLES 2048
/* Then the i860 runs 16 cycles every SPIN_SLEEP_US. */
#define SPIN_SLEEP_US 100

/* Cycles the i860 ran, for the speed report (platform.c). */
static long long ran_cycles;

static int i860_thread(void *data)
{
	i860_cpu_device *cpu = (i860_cpu_device *) data;
	NextDimension *nd = cpu->nd;
	UINT32 lo = 0, hi = 0;
	UINT32 regs[32];
	int spin = 0;

	host_thread_priority(0);
	while (nd->handle_msgs()) {
		UINT32 pc;
		if (cpu->is_halted()) {
			spin = 0;
			host_sleep_ms(100);
			continue;
		}
		if (host_atomic_get(&cpu->i860cycles) <= 0) {
			host_sleep_ms(1);
			continue;
		}
		for (int i = 16; --i >= 0;)
			cpu->run_cycle();
		host_atomic_add(&cpu->i860cycles, -16);
		__atomic_fetch_add(&ran_cycles, 16, __ATOMIC_RELAXED);

		/* Still in the same few instructions, with the same registers? */
		pc = cpu->m_pc;
		if (spin > 0) {
			UINT32 new_lo = pc < lo ? pc : lo, new_hi = pc > hi ? pc : hi;
			if (new_hi - new_lo < SPIN_BYTES
			    && memcmp(regs, cpu->m_iregs, sizeof regs) == 0) {
				lo = new_lo;
				hi = new_hi;
				if (spin < SPIN_CYCLES)
					spin += 16;
				else
					host_sleep_us(SPIN_SLEEP_US);
				continue;
			}
		}
		/* Something else: start looking again from here. */
		lo = hi = pc;
		memcpy(regs, cpu->m_iregs, sizeof regs);
		spin = 16;
	}
	return 0;
}

extern "C" {

thread_func_t bmnext_i860_thread(thread_func_t func)
{
	return func == i860_cpu_device::thread ? i860_thread : func;
}

long long bmnext_i860_cycles(void)
{
	return __atomic_exchange_n(&ran_cycles, 0, __ATOMIC_RELAXED);
}

}
