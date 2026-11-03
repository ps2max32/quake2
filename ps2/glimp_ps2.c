/* glimp_ps2.c: the GLimp_* functions of ref_gl on libgl
   (libs/libgl): the TV's screen (NTSC 640 x 448, PAL 640 x 512), or
   1080i with vid_hires 1, the game's window 960 x 540 then (its 2D, in
   pixels, twice as large on the screen); beam racing with vid_beam 1,
   24-bit colour with vid_rgb24 1 (then, in the TV's mode). The mode is set
   as the refresh starts (vid_ps2.c restarts it when they change). */

#include <mxgl.h>

#include "../ref_gl/gl_local.h"

int GLimp_Init(void *hinstance, void *hWnd) {
	mxgl_init();
	/* gl_ps2texquality: -1 (the default) the textures' top levels left out
	   only while GS memory overflows or the swap file is read
	   (MXGL_TEX_AUTO); 0, 1, 2: that many always */
	mxgl_texture_quality((int)ri.Cvar_Get("gl_ps2texquality", "-1", CVAR_ARCHIVE)->value);
	/* its limits (mxgl_texture_limits): GS memory high and low (% of the
	   room), swap-ins a second */
	int hi = 120, lo = 80, sw = 30;
	sscanf(ri.Cvar_Get("gl_ps2texlimits", "120 80 30", CVAR_ARCHIVE)->string, "%d%*c%d%*c%d", &hi, &lo, &sw);
	mxgl_texture_limits(hi, lo, sw);
	return true;
}

/* the mode of vid_hires, and the window of it */
int GLimp_SetMode(int *pwidth, int *pheight, int mode, qboolean fullscreen) {
	int hires = ri.Cvar_Get("vid_hires", "0", 0)->value == 1;
	cvar_t *beam = ri.Cvar_Get("vid_beam", "0", CVAR_ARCHIVE);
	int flags = (beam->value ? MXGL_BEAM : 0) |
	            (ri.Cvar_Get("vid_rgb24", "0", CVAR_ARCHIVE)->value ? MXGL_RGB24 : 0);
	if (mxgl_set_mode((hires ? MXGL_1080I : MXGL_SD) | flags) < 0) {
		ri.Con_Printf(PRINT_ALL, "beam racing: not enough memory\n");
		ri.Cvar_Set("vid_beam", "0");
		beam->modified = false;
	}
	/* coloured light: the lightmaps' products a channel at a time
	   (gl_rsurf.c keeps them in 8 bits a channel) */
	mxgl_color_light(ri.Cvar_Get("gl_ps2colorlight", "1", CVAR_ARCHIVE)->value != 0);
	int w, h;
	mxgl_screen(&w, &h);
	if (hires)
		w = 960, h = 540;
	mxgl_window(w, h);
	*pwidth = w;
	*pheight = h;
	ri.Vid_NewWindow(w, h);
	return rserr_ok;
}

void GLimp_Shutdown(void) {}
void GLimp_BeginFrame(float camera_separation) {}

void GLimp_EndFrame(void) {
	mxgl_end_frame();
	mxgl_frame_done();
}

void GLimp_AppActivate(qboolean active) {}
void GLimp_EnableLogging(qboolean enable) {}
void GLimp_LogNewFrame(void) {}

/* x86 assembly on the PC (linux/math.s) */
long Q_ftol(float f) { return (long)f; }
