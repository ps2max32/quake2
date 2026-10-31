/* glimp_ps2.c: the GLimp_* functions of ref_gl on maxsdk_gl
   (libs/maxsdk_gl): one 640 x 448 screen. */

#include <mxgl.h>

#include "../ref_gl/gl_local.h"

int GLimp_Init(void *hinstance, void *hWnd) {
	mxgl_init();
	return true;
}

int GLimp_SetMode(int *pwidth, int *pheight, int mode, qboolean fullscreen) {
	*pwidth = MXGL_WIDTH;
	*pheight = MXGL_HEIGHT;
	ri.Vid_NewWindow(*pwidth, *pheight);
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
