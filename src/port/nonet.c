/*
 * nonet.c - the NeXT's network, unplugged, for BMNeXT.
 *
 * Previous connects the NeXT's Ethernet to the host through SLiRP
 * (enet_slirp.c and src/slirp/, with its NFS and NetInfo servers) or
 * libpcap (enet_pcap.c). SLiRP runs on threads of its own and uses the C
 * library's sockets; Circle has neither, so BMNeXT leaves the network out
 * for now: the NeXT sees its Ethernet with no cable plugged in. Packets it
 * sends are dropped, and none arrive.
 *
 * This file is part of BMNeXT, a bare metal NeXT emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#include "main.h"
#include "enet_slirp.h"

void enet_slirp_queue_poll(void)
{
}

void enet_slirp_input(uint8_t *pkt, int pkt_len)
{
}

void enet_slirp_stop(void)
{
}

void enet_slirp_start(uint8_t *mac)
{
	static int told;
	if (!told) {
		printf("BMNeXT: the network is not emulated yet\n");
		told = 1;
	}
}

void enet_slirp_uninit(void)
{
}
