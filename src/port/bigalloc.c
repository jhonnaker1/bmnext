/*
 * bigalloc.c - big blocks of memory, used again once freed.
 *
 * Circle's heap keeps freed blocks in lists of a few sizes, the largest
 * 512K (HEAP_BLOCK_BUCKET_SIZES in Circle's sysconfig.h); a bigger block
 * is lost when freed. Previous frees and allocates the NeXT's memory (up to
 * 128 MB, with malloc(): malloc_aligned() in cpu/maccess.h, src/port/
 * config.h) and a
 * NeXTdimension board's (68 MB) each time the machine is set up again
 * (Machine, a new memory size, a CPU, the board): after a few times there
 * was no memory left (std::bad_alloc).
 *
 * So the kernel is linked with --wrap=malloc --wrap=free (the Makefile):
 * the big blocks malloc() gives are noted here, and when one is freed it is
 * kept, to be given again by malloc() for a block that fits in it. Every
 * other block goes to Circle as before. (realloc() and calloc() aren't
 * wrapped: a noted block given to realloc() is freed by Circle, and is
 * never noted as free here, which is harmless.)
 *
 * Only the Pi needs this: the PC test harness's C library reuses memory.
 *
 * This file is part of BMNeXT, a bare metal NeXT emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#ifndef BMNEXT_HOST_LIBC

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Blocks bigger than Circle's largest bucket. */
#define BIG_BYTES 0x80000
#define MAX_BLOCKS 64

void *__real_malloc(size_t size);
void __real_free(void *block);
void *__wrap_malloc(size_t size);
void __wrap_free(void *block);

/* Entries are only ever added (under LOCK, from malloc()); free()
   finds its block without the lock, as it may come from anywhere (another
   core, an interrupt). An entry's IN_USE is claimed atomically. */
static struct {
	void *block;
	size_t size;
	int in_use;
} blocks[MAX_BLOCKS];
static int count;
static volatile int lock;

void *__wrap_malloc(size_t size)
{
	int i, n, best;
	void *block;
	if (size <= BIG_BYTES)
		return __real_malloc(size);
	for (;;) {
		/* The smallest kept block it fits in. */
		n = __atomic_load_n(&count, __ATOMIC_ACQUIRE);
		best = -1;
		for (i = 0; i < n; i++)
			if (!__atomic_load_n(&blocks[i].in_use, __ATOMIC_ACQUIRE)
			    && blocks[i].size >= size
			    && (best < 0 || blocks[i].size < blocks[best].size))
				best = i;
		if (best < 0)
			break;
		int free_entry = 0;
		if (__atomic_compare_exchange_n(&blocks[best].in_use, &free_entry, 1, 0,
		                                __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
			return blocks[best].block;
		}
	}
	block = __real_malloc(size);
	if (block == NULL)
		return NULL;
	while (__atomic_exchange_n(&lock, 1, __ATOMIC_ACQUIRE))
		;
	n = count;
	if (n < MAX_BLOCKS) {
		blocks[n].block = block;
		blocks[n].size = size;
		blocks[n].in_use = 1;
		__atomic_store_n(&count, n + 1, __ATOMIC_RELEASE);
	}
	__atomic_store_n(&lock, 0, __ATOMIC_RELEASE);
	return block;
}

void __wrap_free(void *block)
{
	int i, n;
	if (block == NULL)
		return;
	n = __atomic_load_n(&count, __ATOMIC_ACQUIRE);
	for (i = 0; i < n; i++)
		if (blocks[i].block == block) {
			__atomic_store_n(&blocks[i].in_use, 0, __ATOMIC_RELEASE);
			return;
		}
	__real_free(block);
}

#endif
