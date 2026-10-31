/* net_ps2.c: the network of Quake 2 on maxsdk: the loopback only (the
   single player game: client and server in one program), from
   linux/net_udp.c. */

#include "../qcommon/qcommon.h"

#define MAX_LOOPBACK 4

typedef struct {
	byte data[MAX_MSGLEN];
	int datalen;
} loopmsg_t;

typedef struct {
	loopmsg_t msgs[MAX_LOOPBACK];
	int get, send;
} loopback_t;

static loopback_t loopbacks[2];
netadr_t net_local_adr;

qboolean NET_CompareAdr(netadr_t a, netadr_t b) {
	return a.type == b.type && (a.type == NA_LOOPBACK || (!memcmp(a.ip, b.ip, 4) && a.port == b.port));
}

qboolean NET_CompareBaseAdr(netadr_t a, netadr_t b) {
	return a.type == b.type && (a.type == NA_LOOPBACK || !memcmp(a.ip, b.ip, 4));
}

char *NET_AdrToString(netadr_t a) {
	static char s[64];
	if (a.type == NA_LOOPBACK)
		return "loopback";
	Com_sprintf(s, sizeof s, "%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3], a.port);
	return s;
}

char *NET_BaseAdrToString(netadr_t a) { return NET_AdrToString(a); }

qboolean NET_StringToAdr(char *s, netadr_t *a) {
	if (!strcmp(s, "localhost")) {
		memset(a, 0, sizeof *a);
		a->type = NA_LOOPBACK;
		return true;
	}
	return false;
}

qboolean NET_IsLocalAddress(netadr_t adr) { return adr.type == NA_LOOPBACK; }

static qboolean NET_GetLoopPacket(netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message) {
	loopback_t *loop = &loopbacks[sock];
	if (loop->send - loop->get > MAX_LOOPBACK)
		loop->get = loop->send - MAX_LOOPBACK;
	if (loop->get >= loop->send)
		return false;
	int i = loop->get & (MAX_LOOPBACK - 1);
	loop->get++;
	memcpy(net_message->data, loop->msgs[i].data, loop->msgs[i].datalen);
	net_message->cursize = loop->msgs[i].datalen;
	memset(net_from, 0, sizeof *net_from);
	net_from->type = NA_LOOPBACK;
	return true;
}

qboolean NET_GetPacket(netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message) {
	return NET_GetLoopPacket(sock, net_from, net_message);
}

void NET_SendPacket(netsrc_t sock, int length, void *data, netadr_t to) {
	if (to.type != NA_LOOPBACK)
		return;
	loopback_t *loop = &loopbacks[sock ^ 1];
	int i = loop->send & (MAX_LOOPBACK - 1);
	loop->send++;
	memcpy(loop->msgs[i].data, data, length);
	loop->msgs[i].datalen = length;
}

void NET_Config(qboolean multiplayer) {}
void NET_Init(void) {}
void NET_Shutdown(void) {}
void NET_Sleep(int msec) {}
char *NET_ErrorString(void) { return "no network"; }
