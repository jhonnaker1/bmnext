/*
 * host_next.c - Previous's host functions (src/includes/host.h) for BMNeXT.
 *
 * Replaces src/gui-sdl/sdlhost.c. Previous asks its host for threads,
 * locks, atomics, sleeping and a clock. BMNeXT runs Previous on one core,
 * on one thread (config.h's ENABLE_RENDERING_THREAD runs the CPU on the
 * main thread), and builds none of the parts that start threads: the
 * NeXTdimension's i860 (src/dimension/), the network (src/slirp/,
 * enet_slirp.c, enet_pcap.c) and the SDL front end's repaint thread
 * (src/gui-sdl/). So a lock never has to wait, an atomic is a plain
 * variable, and starting a thread is a mistake: it is refused (NULL) and
 * logged.
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

/* ---- Locks and atomics (one thread: nothing waits) ---------------------- */

void host_lock(lock_t *lock)
{
	*lock = 1;
}

int host_trylock(lock_t *lock)
{
	*lock = 1;
	return 1;
}

void host_unlock(lock_t *lock)
{
	*lock = 0;
}

int host_atomic_set(atomic_int *a, int newValue)
{
	int old = a->value;
	a->value = newValue;
	return old;
}

int host_atomic_get(atomic_int *a)
{
	return a->value;
}

int host_atomic_add(atomic_int *a, int value)
{
	int old = a->value;
	a->value += value;
	return old;
}

int host_atomic_cas(atomic_int *a, int oldValue, int newValue)
{
	if (a->value != oldValue)
		return 0;
	a->value = newValue;
	return 1;
}

/* ---- Threads (none) ------------------------------------------------------- */

thread_t *host_thread_create(thread_func_t func, const char *name, void *data)
{
	printf("BMNeXT: Previous asked for a thread (%s): not available\n",
	       name ? name : "?");
	return NULL;
}

void host_thread_priority(int priority)
{
}

int host_thread_wait(thread_t *thread)
{
	return 0;
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
	host_sleep_us(ms * 1000u);
}

/* Previous's takes 64 bits, the kernel's 32: long sleeps are cut up. */
void host_sleep_us64(uint64_t us)
{
	while (us > 0) {
		unsigned part = us > 1000000u ? 1000000u : (unsigned) us;
		host_sleep_us(part);
		us -= part;
	}
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
   network and display): as far as Previous is concerned, one. */
int host_num_cpus(void)
{
	return 1;
}
