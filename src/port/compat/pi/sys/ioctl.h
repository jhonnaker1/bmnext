/*
 * sys/ioctl.h - for SLiRP (src/slirp/), on the Pi: newlib for Circle has none.
 * What SLiRP needs of it (struct iovec, FIONBIO, FIONREAD) is in bmnet.h,
 * included before every SLiRP file (src/port/compat/bmnet.h).
 *
 * This file is part of BMNeXT, a bare metal NeXT emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#include "../../bmnet.h"
