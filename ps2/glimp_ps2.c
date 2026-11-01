/* glimp_ps2.c: the GLimp_* functions of ref_gl on libgl
   (libs/libgl): the TV's screen (NTSC 640 x 448, PAL 640 x 512), or
   1080i with vid_hires 1, the game's window 960 x 540 then (its 2D, in
   pixels, twice as large on the screen). The mode is set as the refresh
   starts (vid_ps2.c restarts it when vid_hires changes). */

#include <mxgl.h>

#include "../ref_gl/gl_local.h"

int GLimp_Init(void *hinstance, void *hWnd) {
	mxgl_init();
	return true;
}

/* the mode of vid_hires, and the window of it */
int GLimp_SetMode(int *pwidth, int *pheight, int mode, qboolean fullscreen) {
	int hires = ri.Cvar_Get("vid_hires", "0", 0)->value == 1;
	if (mxgl_set_mode(hires ? MXGL_1080I : MXGL_SD) < 0) {
		ri.Con_Printf(PRINT_ALL, "1080i: not enough memory\n");
		ri.Cvar_Set("vid_hires", "0");
		hires = 0;
	}
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
