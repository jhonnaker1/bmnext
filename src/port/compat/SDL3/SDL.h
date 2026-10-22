/*
 * SDL3/SDL.h - the little of SDL that Previous's core needs, for BMNeXT.
 *
 * Previous's host.h (src/includes/host.h) names its thread, lock and
 * atomic types after SDL's, and includes SDL to get them; nothing else in
 * the parts of Previous BMNeXT builds uses SDL (src/gui-sdl/ is replaced
 * by src/port/). On Circle there is one thread: the types here are plain
 * counters, and host_next.c implements host.h's functions on them.
 *
 * This file is part of BMNeXT, a bare metal NeXT emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#ifndef BMNEXT_SDL_STANDIN_H_
#define BMNEXT_SDL_STANDIN_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { volatile int value; } SDL_AtomicInt;
typedef int SDL_SpinLock;
typedef struct SDL_Thread SDL_Thread;
typedef int (*SDL_ThreadFunction)(void *data);
typedef struct SDL_Semaphore { uint32_t count; } SDL_Semaphore;
typedef struct SDL_Mutex { int locked; } SDL_Mutex;

#ifdef __cplusplus
}
#endif

#endif /* BMNEXT_SDL_STANDIN_H_ */
