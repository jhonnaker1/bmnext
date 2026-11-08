/*
 * host_next.c - Previous's host functions (src/includes/host.h) for BMNeXT.
 *
 * Replaces src/gui-sdl/sdlhost.c. Previous asks its host for threads,
 * locks, atomics, sleeping and a clock. BMNeXT runs Previous's CPU on one
 * core (config.h's ENABLE_RENDERING_THREAD runs it on the main thread), and
 * builds none of the parts that start threads but one (the network's,
 * enet_slirp.c and enet_pcap.c, and the SDL front end's repaint thread,
 * src/gui-sdl/, are not built): the NeXTdimension's i860. Where there is a
 * helper core (src/host.h: the Pi 5's core 3), host_num_cpus() says two,
 * Previous runs the i860 on a thread of its own, and that thread runs on
 * the helper; else the i860 runs on the CPU's thread. The two share the
 * board through Previous's atomics, which are real ones here, and the
 * locks too. The i860's log lines are queued for the CPU's core to print.
 *
 * The clock and sleeping are the kernel's (src/host.h): Previous paces the
 * emulation to real time by sleeping (timing.c, Timing_Sync()).
 *
 * This file is part of BMNeXT, a bare metal NeXT emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

/* Previous's host_sleep_us() is host_sleep_us64() (see previous.mk). */
#define host_sleep_us host_sleep_us64
#include "main.h"
#include "host.h"
#undef host_sleep_us

#include "../host.h"
#include "bmnext.h"
#include "log.h"

#include <stdarg.h>

/* ---- Locks and atomics (shared with the helper core) ---------------------- */

void host_lock(lock_t *lock)
{
	while (__atomic_exchange_n(lock, 1, __ATOMIC_ACQUIRE))
		;
}

int host_trylock(lock_t *lock)
{
	return !__atomic_exchange_n(lock, 1, __ATOMIC_ACQUIRE);
}

void host_unlock(lock_t *lock)
{
	__atomic_store_n(lock, 0, __ATOMIC_RELEASE);
}

int host_atomic_set(atomic_int *a, int newValue)
{
	return __atomic_exchange_n(&a->value, newValue, __ATOMIC_SEQ_CST);
}

int host_atomic_get(atomic_int *a)
{
	return __atomic_load_n(&a->value, __ATOMIC_SEQ_CST);
}

int host_atomic_add(atomic_int *a, int value)
{
	return __atomic_fetch_add(&a->value, value, __ATOMIC_SEQ_CST);
}

int host_atomic_cas(atomic_int *a, int oldValue, int newValue)
{
	return __atomic_compare_exchange_n(&a->value, &oldValue, newValue, 0,
	                                   __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

/* ---- Threads: one, on the helper core ------------------------------------- */

/* Previous's thread_t is SDL's, which it never looks into. */
static struct {
	thread_func_t func;
	void *data;
} helper;

static void helper_task(void *unused)
{
	helper.func(helper.data);
}

/* nd_i860.cpp's i860 thread, which skips its wait loops, for Previous's. */
thread_func_t bmnext_i860_thread(thread_func_t func);

thread_t *host_thread_create(thread_func_t func, const char *name, void *data)
{
	helper.func = bmnext_i860_thread(func);
	helper.data = data;
	if (host_helper_start(helper_task, NULL) != 0) {
		printf("BMNeXT: Previous asked for a thread (%s): not available\n",
		       name ? name : "?");
		return NULL;
	}
	printf("BMNeXT: %s on the helper core\n", name ? name : "a thread");
	return (thread_t *) &helper;
}

void host_thread_priority(int priority)
{
}

/* Waits in real time (the PC simulator's own clock is virtual), letting
   the kernel's tasks run: the i860's thread may be in a 100 ms sleep. */
int host_thread_wait(thread_t *thread)
{
	if (thread == (thread_t *) &helper)
		while (host_helper_busy()) {
			host_yield();
			host_helper_sleep_us(1000);
		}
	return 0;
}

/* ---- The helper's log lines ------------------------------------------------ */

/* Previous's C++ files (the NeXTbus and NeXTdimension) log through here
   (compat/previous_prefix.h): the CPU's core prints at once, the helper's
   lines wait in a queue (one writer, one reader) for the CPU's core. */
#define HELPER_LOG_LINES 16
static struct {
	LOGTYPE type;
	char text[160];
} helper_log[HELPER_LOG_LINES];
static unsigned helper_log_head, helper_log_tail;

static void helper_log_drain(void)
{
	unsigned tail = __atomic_load_n(&helper_log_tail, __ATOMIC_RELAXED);
	while (tail != __atomic_load_n(&helper_log_head, __ATOMIC_ACQUIRE)) {
		unsigned i = tail % HELPER_LOG_LINES;
		Log_PrintfInt(helper_log[i].type, "%s", helper_log[i].text);
		tail++;
		__atomic_store_n(&helper_log_tail, tail, __ATOMIC_RELEASE);
	}
}

void bmnext_log_from_cxx(LOGTYPE type, const char *format, ...)
{
	char text[160];
	va_list args;
	va_start(args, format);
	vsnprintf(text, sizeof text, format, args);
	va_end(args);
	if (host_on_helper()) {
		unsigned head = __atomic_load_n(&helper_log_head, __ATOMIC_RELAXED);
		if (head - __atomic_load_n(&helper_log_tail, __ATOMIC_ACQUIRE) < HELPER_LOG_LINES) {
			unsigned i = head % HELPER_LOG_LINES;
			helper_log[i].type = type;
			snprintf(helper_log[i].text, sizeof helper_log[i].text, "%s", text);
			__atomic_store_n(&helper_log_head, head + 1, __ATOMIC_RELEASE);
		}
		return;
	}
	helper_log_drain();
	Log_PrintfInt(type, "%s", text);
}

semaphore_t *host_semaphore_create(uint32_t value)
{
	semaphore_t *s = calloc(1, sizeof(*s));
	if (s)
		s->count = value;
	return s;
}

void host_semaphore_signal(semaphore_t *semaphore)
{
	if (semaphore)
		semaphore->count++;
}

/* Nothing else could signal it: waiting would never end. */
int host_semaphore_wait_timeout(semaphore_t *semaphore, int32_t ms)
{
	if (semaphore && semaphore->count > 0) {
		semaphore->count--;
		return 0;
	}
	return 1;
}

void host_semaphore_destroy(semaphore_t *semaphore)
{
	free(semaphore);
}

mutex_t *host_mutex_create(void)
{
	return calloc(1, sizeof(mutex_t));
}

void host_mutex_lock(mutex_t *mutex)
{
	if (mutex)
		mutex->locked = 1;
}

void host_mutex_unlock(mutex_t *mutex)
{
	if (mutex)
		mutex->locked = 0;
}

void host_mutex_destroy(mutex_t *mutex)
{
	free(mutex);
}

/* ---- Time ----------------------------------------------------------------- */

void host_sleep_ms(uint32_t ms)
{
	host_sleep_us64(ms * 1000u);
}

/* Previous's takes 64 bits, the kernel's 32: long sleeps are cut up. On
   the helper core, without the kernel's scheduler. */
void host_sleep_us64(uint64_t us)
{
	double t;
	if (host_on_helper()) {
		while (us > 0) {
			unsigned part = us > 1000000u ? 1000000u : (unsigned) us;
			host_helper_sleep_us(part);
			us -= part;
		}
		return;
	}
	helper_log_drain();
	t = host_time();
	while (us > 0) {
		unsigned part = us > 1000000u ? 1000000u : (unsigned) us;
		host_sleep_us(part);
		us -= part;
	}
	bmnext_prof[PROF_SLEEP] += host_time() - t;
}

/* Microseconds. */
uint64_t host_get_counter(void)
{
	return (uint64_t) (host_time() * 1e6);
}

uint64_t host_get_counter_frequency(void)
{
	return 1000000u;
}

/* The emulator has one core to itself (the others run the kernel's USB,
   network and display), and, for the NeXTdimension's i860, the helper core
   where there is one (src/host.h): Previous runs the i860 on a thread of
   its own when it is told of more than one. */
int host_num_cpus(void)
{
	return BMNEXT_HAS_DIMENSION && host_helper_available() ? 2 : 1;
}
