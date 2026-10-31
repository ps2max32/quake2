/* sys_ps2.c: the system layer of Quake 2 on maxsdk: main, time, the hunk
   (malloc, then the unused end given back), files (stdio on host: through
   ps2_file.c), the game module linked in (game_GetGameAPI: game/ linked
   into one object, Makefile), errors. */

#include <mx/mx.h>
#include <mxgl.h>

#include "../qcommon/qcommon.h"

void mx_exit(int status) __attribute__((noreturn));

/* frames to run before the program ends by itself (0: never) with a
   picture, and more of the command line; set by the Makefile */
#include "runcfg.h"

static void ps2_end(int status) {
	mx_printf("@@PS2TEST_END@@ status=%d\n", status);
	mx_exit(status);
}

/* ---- the console and errors ---------------------------------------------- */

void Sys_ConsoleOutput(char *string) {
	char line[1024];
	int n = 0;
	for (; *string && n < (int)sizeof line - 1; string++)
		line[n++] = *string & 0x7f; /* Quake's high-bit characters as plain ones */
	line[n] = 0;
	mx_printf("%s", line);
}

void Sys_Error(char *error, ...) {
	va_list ap;
	char text[1024];
	va_start(ap, error);
	vsnprintf(text, sizeof text, error, ap);
	va_end(ap);
	mx_printf("Sys_Error: %s\n", text);
	ps2_end(1);
}

void Sys_Quit(void) {
	mx_printf("Sys_Quit\n");
	ps2_end(0);
}

char *Sys_ConsoleInput(void) { return NULL; }
void Sys_AppActivate(void) {}
void Sys_CopyProtect(void) {}
char *Sys_GetClipboardData(void) { return NULL; }
void Sys_Init(void) {}

/* ---- time ------------------------------------------------------------------ */

int curtime;

int Sys_Milliseconds(void) {
	static u32 last, high;
	static u64 base;
	u32 c = mx_count();
	if (c < last)
		high++;
	last = c;
	u64 t = (u64)high << 32 | c;
	if (!base)
		base = t;
	curtime = (int)((t - base) / (MX_EE_HZ / 1000));
	return curtime;
}

/* ---- the hunk: as much as asked for, or as much as there is; Hunk_End
   gives the rest back (realloc to the size used) ---------------------- */

static byte *membase;
static int maxhunksize, curhunksize;

/* the largest block malloc gives (in steps of 64 KiB) */
static int largest_free(void) {
	int lo = 0, hi = 32 << 20;
	while (hi - lo > 64 * 1024) {
		int mid = (lo + hi) / 2;
		void *volatile p = malloc(mid); /* volatile: not optimised away */
		if (p)
			free(p), lo = mid;
		else
			hi = mid;
	}
	return lo;
}

/* what loading needs from malloc while a hunk is open (the file being
   read, its pak buffers, textures) */
#define HUNK_HEADROOM (3 << 20)

void *Hunk_Begin(int maxsize) {
	int room = largest_free() - HUNK_HEADROOM;
	maxhunksize = maxsize < room ? maxsize : room;
	curhunksize = 0;
	membase = maxhunksize > 0 ? malloc(maxhunksize + 32) : NULL;
	if (!membase)
		Sys_Error("Hunk_Begin: no memory for %d bytes", maxsize);
	*(int *)membase = maxhunksize;
	return membase + 32;
}

void *Hunk_Alloc(int size) {
	size = (size + 31) & ~31;
	if (curhunksize + size > maxhunksize)
		Sys_Error("Hunk_Alloc overflow (%d of %d)", curhunksize + size, maxhunksize);
	byte *buf = membase + 32 + curhunksize;
	curhunksize += size;
	return buf;
}

int Hunk_End(void) {
	/* in place: picolibc's realloc frees the end of a block it shrinks */
	if (realloc(membase, curhunksize + 32) != membase)
		Sys_Error("Hunk_End: the block moved");
	if (curhunksize > 1 << 20)
		mx_printf("ps2: hunk %d KiB, %d KiB free after\n", curhunksize >> 10, largest_free() >> 10);
	return curhunksize;
}

void Hunk_Free(void *base) {
	if (base)
		free((byte *)base - 32);
}

/* ---- files -------------------------------------------------------------------- */

void Sys_Mkdir(char *path) {}
char *Sys_FindFirst(char *path, unsigned musthave, unsigned canthave) { return NULL; }
char *Sys_FindNext(unsigned musthave, unsigned canthave) { return NULL; }
void Sys_FindClose(void) {}

char *strlwr(char *s) {
	for (char *p = s; *p; p++)
		if (*p >= 'A' && *p <= 'Z')
			*p += 'a' - 'A';
	return s;
}

/* ---- the game module ------------------------------------------------------------ */

void *game_GetGameAPI(void *import);

void Sys_UnloadGame(void) {}
void *Sys_GetGameAPI(void *parms) { return game_GetGameAPI(parms); }

/* ---- main ------------------------------------------------------------------------- */

int main(int argc, char **argv) {
	/* the command line: Quake 2's settings for the PS2, then PS2_ARGS */
	static char line[512] = "quake2 +set basedir host: +set s_initsound 0 +set vid_ref gl "
	                        "+set gl_texturemode GL_LINEAR_MIPMAP_LINEAR " PS2_ARGS;
	static char *args[64];
	int n = 0;
	for (char *p = strtok(line, " "); p && n < 63; p = strtok(NULL, " "))
		args[n++] = p;
	mx_printf("Quake 2 on maxsdk: heap %p..%p, %d KiB free\n", mx_heap_start(), mx_heap_end(),
	          largest_free() >> 10);
	Qcommon_Init(n, args);

	int oldtime = Sys_Milliseconds(), frames = 0;
	for (;;) {
		int newtime, time;
		do {
			newtime = Sys_Milliseconds();
			time = newtime - oldtime;
		} while (time < 1);
		Qcommon_Frame(time);
		oldtime = newtime;
		/* frames: those drawn (Qcommon_Frame also runs without drawing) */
		if ((int)mx_gs.frames == frames)
			continue;
		frames = (int)mx_gs.frames;
		if (frames % 60 == 0)
			mx_printf("ps2: frame %d at %d ms\n", frames, newtime);
		if (PS2_FRAMES && frames >= PS2_FRAMES) {
			int r = mxgl_screenshot("host:quake2.tga");
			mx_printf("ps2: %d frames, picture host:quake2.tga: %d\n", frames, r);
			ps2_end(0);
		}
	}
	return 0;
}

/* ---- libc odds ---------------------------------------------------------------------- */

#include <sys/stat.h>
mode_t umask(mode_t mask) { return 0; }
