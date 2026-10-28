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

/* A UDP packet's checksum, over IP's pseudo header (IP header IP, IP_LEN
   bytes in all): 0xffff when it is right. */
static unsigned udp_checksum(const unsigned char *ip, int ip_len)
{
	int ihl = (ip[0] & 15) * 4, udp_len = ip_len - ihl, i;
	uint32_t sum = 0;
	for (i = 12; i < 20; i += 2)            /* source and destination */
		sum += (uint32_t) (ip[i] << 8 | ip[i + 1]);
	sum += 17 + (uint32_t) udp_len;
	for (i = 0; i + 1 < udp_len; i += 2)
		sum += (uint32_t) (ip[ihl + i] << 8 | ip[ihl + i + 1]);
	if (udp_len & 1)
		sum += (uint32_t) ip[ihl + udp_len - 1] << 8;
	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);
	return sum;   /* 0xffff if right (or the packet has no checksum) */
}

/* SLiRP's routing table broadcasts (rip.c) give the address family as the
   C library's AF_INET, which RIP takes to be BSD's (2). Circle's C library
   has 1, which the NeXT's routed doesn't know, so it never learned its
   default route (10.0.2.2) on the Pi. Here a RIP packet to the NeXT gets
   2, and its UDP checksum again. */
#define RIP_AF_INET 2

static void fix_rip_family(unsigned char *pkt, int len)
{
	unsigned char *ip = pkt + 14, *udp, *e;
	int ihl, ip_len, n;
	unsigned sum;
	if (AF_INET == RIP_AF_INET || len < 14 + 28 || pkt[12] != 0x08 ||
	    pkt[13] != 0x00 || ip[9] != 17)
		return;
	ihl = (ip[0] & 15) * 4;
	ip_len = ip[2] << 8 | ip[3];
	udp = ip + ihl;
	if ((udp[0] << 8 | udp[1]) != 520 || ip_len > len - 14)
		return;
	/* After the UDP header: command, version, two zeros, then entries of
	   20 bytes, each starting with its family. */
	for (e = udp + 8 + 4, n = 0; e + 20 <= ip + ip_len; e += 20)
		if ((e[0] << 8 | e[1]) == AF_INET) {
			e[0] = 0;
			e[1] = RIP_AF_INET;
			n++;
		}
	if (n == 0 || (udp[6] == 0 && udp[7] == 0))
		return;
	udp[6] = udp[7] = 0;
	sum = ~udp_checksum(ip, ip_len) & 0xffff;
	if (sum == 0)
		sum = 0xffff;
	udp[6] = (unsigned char) (sum >> 8);
	udp[7] = (unsigned char) sum;
}

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
	fix_rip_family(p->data, pkt_len);
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
		/* The NeXT's routed learns its default route (10.0.2.2) from it. */
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
