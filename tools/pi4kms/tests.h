//
// tests.h
//
// Shared by the direct display driver's PC tests (see test.cpp).
//
// This file is part of BMNeXT, a bare metal NeXT emulator for
// the Raspberry Pi. See LICENSE for licensing details.
//

#ifndef BMNEXT_TOOLS_PI4KMS_TESTS_H_
#define BMNEXT_TOOLS_PI4KMS_TESTS_H_

extern int g_failed;
void Check(bool ok, const char *name, const char *what);

// test_modes.cpp: EDID, video modes, InfoFrames, PHY settings.
void RunModeTests(void);

#endif
