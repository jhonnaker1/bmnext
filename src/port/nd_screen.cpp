/*
 * nd_screen.cpp - the NeXTdimension's screen functions for BMNeXT.
 *
 * Replaces Previous's src/dimension/nd_sdl.cpp, which opens an SDL window
 * of its own for each NeXTdimension board and repaints it from the board's
 * video memory. BMNeXT has one display: screen.c shows either the NeXT's
 * own screen or a board's (Previous's single screen mode,
 * ConfigureParams.Screen.nSingleModeSlot), reading the board's memory
 * through nd_vram_for_slot(). So there is no window here, and these do
 * nothing.
 *
 * This file is part of BMNeXT, a bare metal NeXT emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#include "main.h"
#include "nd_sdl.hpp"

#ifdef ENABLE_RENDERING_THREAD
NDSDL::NDSDL(int slot, uint32_t *vram)
    : slot(slot), vram(vram), ndWindow(NULL), ndRenderer(NULL), ndTexture(NULL),
      doRepaint(false), repaintThread(NULL) {}
#else
NDSDL::NDSDL(int slot, uint32_t *vram)
    : slot(slot), vram(vram), ndWindow(NULL), ndRenderer(NULL), ndTexture(NULL) {}
#endif

void NDSDL::repaint(void) {}
void NDSDL::init(void) {}
void NDSDL::uninit(void) {}
void NDSDL::destroy(void) {}
void NDSDL::resize(float scale) {}
void NDSDL::titlebar(bool show) {}

extern "C" {
#ifndef ENABLE_RENDERING_THREAD
void nd_sdl_repaint(void) {}
#endif
void nd_sdl_titlebar(bool show) {}
void nd_sdl_resize(float scale) {}
void nd_sdl_show(void) {}
void nd_sdl_hide(void) {}
}
