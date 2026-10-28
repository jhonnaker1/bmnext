/*
 * enet_next.c - the NeXT's Ethernet through SLiRP, for BMNeXT.
 *
 * Replaces Previous's enet_slirp.c. SLiRP (src/slirp/) is a NAT router
 * inside the emulator: it answers the NeXT's packets as a network would
 * (BOOTP gives the NeXT its address, 10.0.2.15, and the router's and
 * DNS's), and turns its connections into the host's own. Previous runs it
 * on a thread of its own, every 1.23 ms; BMNeXT has one thread, so here it
 * runs from the emulator's event handler instead (enet_next_poll(), 200
 * times a second of the NeXT's time, from bmnext_tick()), and nothing
 * needs locking. Packets SLiRP has for the NeXT wait in a queue, which
 * Previous's Ethernet chip (ethernet.c) takes one at a time.
 *
 * Previous's NFS, NetInfo, bootparam and other servers (src/slirp/rpc/:
 * NeXTSTEP asks them for its name and settings as it starts, and they
 * share a folder with it) are polled here too, rpc_sockets.c in place of
 * their threads.
 *
 * This file is part of BMNeXT, a bare metal NeXT emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#include "main.h"
#include "log.h"
#include "ethernet.h"
#include "enet_slirp.h"
#include "queue.h"
#include "timing.h"
#include "libslirp.h"
#include "slirp.h"
#include "rpc/rpc.h"

#include "../host.h"

#include "bmnext.h"

#define LOG_EN_SLIRP_LEVEL LOG_DEBUG

/* SLiRP's routing table goes out every 30 s, as in enet_slirp.c. */
#define SLIRP_RIP_SEC 30

static queueADT slirpq;
static int slirp_inited;
static int slirp_started;
static uint64_t rip_due;
/* For the log: the first packets each way show the network works. */
static int seen_in, seen_out;

/* SLiRP can send the NeXT packets (slirp.c asks). */
int slirp_can_output(void)
{
	return slirp_started;
}

/* A packet for the NeXT, from SLiRP: it waits for the Ethernet chip. */
void slirp_output(const unsigned char *pkt, int pkt_len)
{
	struct queuepacket *p = malloc(sizeof *p);
	if (p == NULL || pkt_len > (int) sizeof p->data) {
		free(p);
		return;
	}
	p->len = pkt_len;
	memcpy(p->data, pkt, pkt_len);
	QueueEnter(slirpq, p);
	if (!seen_out) {
		printf("BMNeXT: network: first packet to the NeXT (%d bytes)\n", pkt_len);
		seen_out = 1;
	}
	Log_Printf(LOG_EN_SLIRP_LEVEL, "[SLIRP] Output packet with %i bytes to queue", pkt_len);
}

/* SLiRP's connections: what has arrived, what can be sent, and its
   timers. Never waits. */
void enet_next_poll(void)
{
	fd_set rfds, wfds, xfds;
	struct timeval tv = {0, 0};
	int nfds = -1;
	uint64_t now;

	if (!slirp_started)
		return;
	/* SLiRP reads its DNS server from the host's /etc/resolv.conf, which
	   the Pi hasn't: it is the one the network's DHCP gave, once the
	   network is up. */
	if (dns_addr.s_addr == 0 || dns_addr.s_addr == loopback_addr.s_addr) {
		uint32_t ip;
		if (host_dns_server(&ip)) {
			dns_addr.s_addr = ip;
			printf("BMNeXT: network: DNS %s\n", inet_ntoa(dns_addr));
		}
	}
	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_ZERO(&xfds);
	slirp_select_fill(&nfds, &rfds, &wfds, &xfds);
	if (select(nfds + 1, &rfds, &wfds, &xfds, &tv) >= 0)
		slirp_select_poll(&rfds, &wfds, &xfds);
	/* Previous's servers, which SLiRP may just have passed requests. */
	rpc_sockets_poll();

	now = Timing_GetSaveTime();
	if (rip_due == 0 || now < rip_due - SLIRP_RIP_SEC)
		rip_due = now + SLIRP_RIP_SEC;
	if (now >= rip_due) {
		slirp_rip_broadcast();
		rip_due = now + SLIRP_RIP_SEC;
	}
}

/* ---- Previous's enet_slirp.h functions --------------------------------- */

/* The Ethernet chip is ready for a packet. */
void enet_slirp_queue_poll(void)
{
	if (slirp_started && QueuePeek(slirpq) > 0) {
		struct queuepacket *qp = QueueDelete(slirpq);
		Log_Printf(LOG_EN_SLIRP_LEVEL, "[SLIRP] Getting packet from queue");
		enet_receive(qp->data, qp->len);
		free(qp);
	}
}

/* A packet from the NeXT. */
void enet_slirp_input(uint8_t *pkt, int pkt_len)
{
	if (!slirp_started)
		return;
	if (!seen_in) {
		printf("BMNeXT: network: first packet from the NeXT (%d bytes)\n", pkt_len);
		seen_in = 1;
	}
	slirp_input(pkt, pkt_len);
}

void enet_slirp_stop(void)
{
	if (!slirp_started)
		return;
	Log_Printf(LOG_WARN, "Stopping SLIRP");
	slirp_started = 0;
	while (QueuePeek(slirpq) > 0)
		free(QueueDelete(slirpq));
	QueueDestroy(slirpq);
}

void enet_slirp_uninit(void)
{
	enet_slirp_stop();
	rpc_uninit();
}

void enet_slirp_start(uint8_t *mac)
{
	struct in_addr guest_addr;

	if (!slirp_inited) {
		Log_Printf(LOG_WARN, "Initializing SLIRP");
		slirp_inited = 1;
		slirp_init(&guest_addr);
		/* The host's ports to the NeXT's, as enet_slirp.c has them (no
		   use on the Pi, which has no way in, but harmless). */
		slirp_redir(0, 42320, guest_addr, 20);  /* ftp data */
		slirp_redir(0, 42321, guest_addr, 21);  /* ftp control */
		slirp_redir(0, 42322, guest_addr, 22);  /* ssh */
		slirp_redir(0, 42323, guest_addr, 23);  /* telnet */
		slirp_redir(0, 42380, guest_addr, 80);  /* http */
	}
	if (slirp_inited && !slirp_started) {
		Log_Printf(LOG_WARN, "Starting SLIRP (%02x:%02x:%02x:%02x:%02x:%02x)",
		           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		memcpy(client_ethaddr, mac, 6);
		slirpq = QueueCreate();
		slirp_started = 1;
		rip_due = 0;
	}
	/* Previous's own servers (not running yet: see above), and the
	   addresses SLiRP routes to them. */
	rpc_reset();
}
