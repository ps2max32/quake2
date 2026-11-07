/* net_ps2.c: the network of Quake 2 on maxsdk, from linux/net_udp.c: the
   loopback (the single player game: client and server in one program) and
   UDP by BSD sockets, when the IOP has them (mxsock). No broadcast yet, so the
   server browser finds nothing: connect by address. */

#include "../qcommon/qcommon.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>

int fcntl(int fd, int cmd, ...);

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

static int ip_sockets[2] = {-1, -1};
static int multiplayer;

static void to_sockaddr(netadr_t *a, struct sockaddr_in *s) {
	memset(s, 0, sizeof *s);
	s->sin_family = AF_INET;
	memcpy(&s->sin_addr, a->ip, 4);
	s->sin_port = a->port;
}

static void from_sockaddr(struct sockaddr_in *s, netadr_t *a) {
	a->type = NA_IP;
	memcpy(a->ip, &s->sin_addr, 4);
	a->port = s->sin_port;
}

qboolean NET_CompareAdr(netadr_t a, netadr_t b) {
	if (a.type != b.type)
		return false;
	if (a.type == NA_LOOPBACK)
		return true;
	return !memcmp(a.ip, b.ip, 4) && a.port == b.port;
}

qboolean NET_CompareBaseAdr(netadr_t a, netadr_t b) {
	if (a.type != b.type)
		return false;
	return a.type == NA_LOOPBACK || !memcmp(a.ip, b.ip, 4);
}

char *NET_AdrToString(netadr_t a) {
	static char s[64];
	if (a.type == NA_LOOPBACK)
		return "loopback";
	Com_sprintf(s, sizeof s, "%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3], ntohs(a.port));
	return s;
}

char *NET_BaseAdrToString(netadr_t a) {
	static char s[64];
	if (a.type == NA_LOOPBACK)
		return "loopback";
	Com_sprintf(s, sizeof s, "%i.%i.%i.%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3]);
	return s;
}

/* localhost, or a.b.c.d[:port]: there are no names yet */
qboolean NET_StringToAdr(char *s, netadr_t *a) {
	int part[4] = {0, 0, 0, 0}, i = 0, port = PORT_SERVER;
	char *p = s;
	memset(a, 0, sizeof *a);
	if (!strcmp(s, "localhost")) {
		a->type = NA_LOOPBACK;
		return true;
	}
	for (;; p++) {
		if (*p >= '0' && *p <= '9')
			part[i] = part[i] * 10 + *p - '0';
		else if (*p == '.' && i < 3)
			i++;
		else
			break;
	}
	if (i != 3 || (*p && *p != ':'))
		return false;
	if (*p == ':')
		port = atoi(p + 1);
	a->type = NA_IP;
	for (i = 0; i < 4; i++)
		a->ip[i] = part[i];
	a->port = htons(port);
	return true;
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
	struct sockaddr_in from;
	socklen_t len = sizeof from;
	int n;
	if (NET_GetLoopPacket(sock, net_from, net_message))
		return true;
	if (ip_sockets[sock] < 0)
		return false;
	n = recvfrom(ip_sockets[sock], net_message->data, net_message->maxsize, 0,
	             (struct sockaddr *)&from, &len);
	if (n <= 0)
		return false;
	from_sockaddr(&from, net_from);
	if (n == net_message->maxsize) {
		Com_Printf("Oversize packet from %s\n", NET_AdrToString(*net_from));
		return false;
	}
	net_message->cursize = n;
	return true;
}

void NET_SendPacket(netsrc_t sock, int length, void *data, netadr_t to) {
	if (to.type == NA_LOOPBACK) {
		loopback_t *loop = &loopbacks[sock ^ 1];
		int i = loop->send & (MAX_LOOPBACK - 1);
		loop->send++;
		memcpy(loop->msgs[i].data, data, length);
		loop->msgs[i].datalen = length;
	} else if (to.type == NA_IP && ip_sockets[sock] >= 0) {
		struct sockaddr_in a;
		to_sockaddr(&to, &a);
		sendto(ip_sockets[sock], data, length, 0, (struct sockaddr *)&a, sizeof a);
	}
}

static int open_socket(int port) {
	struct sockaddr_in a;
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		Com_Printf("no sockets: the network is the loopback only\n");
		return -1;
	}
	fcntl(s, F_SETFL, O_NONBLOCK);
	memset(&a, 0, sizeof a);
	a.sin_family = AF_INET;
	a.sin_port = htons(port);
	if (bind(s, (struct sockaddr *)&a, sizeof a) < 0) {
		Com_Printf("UDP port %i is taken\n", port);
		closesocket(s);
		return -1;
	}
	Com_Printf("UDP port %i\n", port);
	return s;
}

void NET_Config(qboolean on) {
	int i;
	if (on == multiplayer)
		return;
	multiplayer = on;
	if (on) {
		ip_sockets[NS_SERVER] = open_socket(Cvar_Get("port", va("%i", PORT_SERVER), CVAR_NOSET)->value);
		ip_sockets[NS_CLIENT] = open_socket(Cvar_Get("clientport", va("%i", PORT_CLIENT), CVAR_NOSET)->value);
		return;
	}
	for (i = 0; i < 2; i++)
		if (ip_sockets[i] >= 0) {
			closesocket(ip_sockets[i]);
			ip_sockets[i] = -1;
		}
}

void NET_Init(void) {}
void NET_Shutdown(void) { NET_Config(false); }
void NET_Sleep(int msec) {}
char *NET_ErrorString(void) { return "network error"; }
