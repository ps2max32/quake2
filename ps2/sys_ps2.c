/* sys_ps2.c: the system layer of Quake 2 on maxsdk: main, time, the hunk
   (malloc, then the unused end given back), files (stdio on host: through
   ps2_file.c), the game module linked in (game_GetGameAPI: game/ linked
   into one object, Makefile), errors. */

#include <mx/mx.h>
#include <mx/prof.h>
#include <mx/vm.h>
#include <mxgl.h>

#include "../qcommon/qcommon.h"

void mx_exit(int status) __attribute__((noreturn));

/* frames to run before the program ends by itself (0: never) with a
   picture, and more of the command line; set by the Makefile */
#include "runcfg.h"

int ps2_cl_time(void);

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

/* ---- the hunk: as much as asked for (virtual memory: the pages not
   used are never touched, as Linux's mmap), Hunk_End gives the rest back
   (realloc to the size used, in place) ---------------------------------- */

static byte *membase;
static int maxhunksize, curhunksize;

void *Hunk_Begin(int maxsize) {
	maxhunksize = maxsize;
	curhunksize = 0;
	membase = malloc(maxhunksize + 32);
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
	/* in place: libmxc's realloc frees the end of a block it shrinks */
	if (realloc(membase, curhunksize + 32) != membase)
		Sys_Error("Hunk_End: the block moved");
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

/* maxsdk's virtual memory (<mx/vm.h>): malloc (the hunks, the zone, the
   sounds) in up to 128 MiB, paged to a swap file next to the game's data;
   what the GS reads by DMA (libgl: arenas, textures) is pinned memory,
   frames taken as needed */
const struct mx_vm_config mx_vm_config = {
	.swap = "host:quake2.swp",
};

int main(int argc, char **argv) {
	/* the command line: Quake 2's settings for the PS2, then PS2_ARGS */
	static char line[512] = "quake2 +set basedir host: +set vid_ref gl "
	                        "+set gl_texturemode GL_LINEAR_MIPMAP_LINEAR "
	                        /* the textures as they are (libgl keeps those of
	                           the shared palette as indices: exact), not
	                           through 16to8.dat; one texture unit (the world's
	                           polygons queue in libgl, faster than two units) */
	                        "+set gl_ext_palettedtexture 0 "
	                        "+set gl_ext_multitexture 0 " PS2_ARGS;
	static char *args[64];
	int n = 0;
	for (char *p = strtok(line, " "); p && n < 63; p = strtok(NULL, " "))
		args[n++] = p;
	mx_printf("Quake 2 on maxsdk: virtual memory %s\n", mx_vm_active() ? "on" : "off");
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
			mx_printf("ps2: frame %d at %d ms, game time %d ms\n", frames, newtime, ps2_cl_time());
#if PS2_PROF == 1
		/* the profile of the second half of the run */
		if (PS2_FRAMES && frames == PS2_FRAMES / 2)
			mx_prof_start(2000);
#elif PS2_PROF
		/* PROF=t: the profile of the game time t .. t + 1 s (a demo's is
		   the same in every run), then the end */
		{
			static int prof_on;
			int gt = ps2_cl_time();
			if (!prof_on && gt >= PS2_PROF && gt < PS2_PROF + 1000)
				mx_prof_start(2000), prof_on = 1;
			if (prof_on == 1 && gt >= PS2_PROF + 1000) {
				mx_prof_stop();
				mx_prof_report(600);
				prof_on = 2;
				ps2_end(0);
			}
		}
#endif
		if (PS2_FRAMES && frames >= PS2_FRAMES) {
#if PS2_PROF == 1
			mx_prof_stop();
			mx_prof_report(600);
#endif
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
