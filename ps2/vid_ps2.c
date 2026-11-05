/* vid_ps2.c: the video and input of Quake 2 on maxsdk: the GL refresh
   (ref_gl, linked in: ref_GetRefAPI) on libgl, and pad 0. From
   null/vid_null.c. */

#include <mx/mx.h>
#include <mx/pad.h>
#include <mxgl.h>

#include "../client/client.h"
#include "../client/qmenu.h"

viddef_t viddef;
unsigned sys_frame_time;
refexport_t re;

refexport_t ref_GetRefAPI(refimport_t rimp);

#define MAXPRINTMSG 4096

void VID_Printf(int print_level, char *fmt, ...) {
	va_list ap;
	char msg[MAXPRINTMSG];
	va_start(ap, fmt);
	vsnprintf(msg, sizeof msg, fmt, ap);
	va_end(ap);
	if (print_level == PRINT_ALL)
		Com_Printf("%s", msg);
	else
		Com_DPrintf("%s", msg);
}

void VID_Error(int err_level, char *fmt, ...) {
	va_list ap;
	char msg[MAXPRINTMSG];
	va_start(ap, fmt);
	vsnprintf(msg, sizeof msg, fmt, ap);
	va_end(ap);
	Com_Error(err_level, "%s", msg);
}

void VID_NewWindow(int width, int height) {
	viddef.width = width;
	viddef.height = height;
}

/* the window is glimp_ps2.c's (the TV's screen, or 960 x 540 in 1080i):
   the mode of gl_mode is not used */
qboolean VID_GetModeInfo(int *width, int *height, int mode) {
	*width = viddef.width;
	*height = viddef.height;
	return true;
}

/* vid_hires 1: 1080i (glimp_ps2.c); the textures at full size in every
   mode: where a room's do not fit in GS memory (1.8 MiB even as 8-bit
   indices: 1080i, the TV's mode without beam racing), libgl leaves their top
   levels out (MXGL_TEX_AUTO, glimp_ps2.c). vid_beam 1: beam racing, which
   leaves 3.6 MiB for textures in the TV's mode (1080i: 1.9); vid_rgb24 1:
   24-bit colour then (TV's mode only) */
static cvar_t *vid_ref, *vid_hires, *vid_beam, *vid_rgb24, *colorlight;
static void VID_HiresPicmip(void) {
	Cvar_SetValue("gl_picmip", 0);
}

static void VID_Restart_f(void) { vid_ref->modified = true; }

void VID_Init(void) {
	refimport_t ri;
	vid_ref = Cvar_Get("vid_ref", "gl", 0);
	vid_hires = Cvar_Get("vid_hires", "0", 0);
	vid_beam = Cvar_Get("vid_beam", "0", CVAR_ARCHIVE);
	vid_rgb24 = Cvar_Get("vid_rgb24", "0", CVAR_ARCHIVE);
	/* coloured lightmaps (ref_gl's gl_rsurf.c, libgl's mxgl_color_light) */
	colorlight = Cvar_Get("gl_ps2colorlight", "1", CVAR_ARCHIVE);
	VID_HiresPicmip();
	Cmd_AddCommand("vid_restart", VID_Restart_f);
	viddef.width = 640;
	viddef.height = 448;
	ri.Cmd_AddCommand = Cmd_AddCommand;
	ri.Cmd_RemoveCommand = Cmd_RemoveCommand;
	ri.Cmd_Argc = Cmd_Argc;
	ri.Cmd_Argv = Cmd_Argv;
	ri.Cmd_ExecuteText = Cbuf_ExecuteText;
	ri.Con_Printf = VID_Printf;
	ri.Sys_Error = VID_Error;
	ri.FS_LoadFile = FS_LoadFile;
	ri.FS_FreeFile = FS_FreeFile;
	ri.FS_Gamedir = FS_Gamedir;
	ri.Vid_NewWindow = VID_NewWindow;
	ri.Cvar_Get = Cvar_Get;
	ri.Cvar_Set = Cvar_Set;
	ri.Cvar_SetValue = Cvar_SetValue;
	ri.Vid_GetModeInfo = VID_GetModeInfo;
	ri.Vid_MenuInit = VID_MenuInit;
	re = ref_GetRefAPI(ri);
	if (re.api_version != API_VERSION)
		Com_Error(ERR_FATAL, "Re has incompatible api_version");
	if (re.Init(NULL, NULL) == -1)
		Com_Error(ERR_FATAL, "Couldn't start refresh");
	vid_ref->modified = false;
	vid_hires->modified = vid_beam->modified = vid_rgb24->modified = colorlight->modified = false;
}

void VID_Shutdown(void) {
	if (re.Shutdown)
		re.Shutdown();
}

/* the refresh again (win32/vid_dll.c's VID_CheckChanges): the mode is set
   as it starts, the textures and models are loaded again (gl_picmip) */
void VID_CheckChanges(void) {
	if (vid_hires->modified || vid_beam->modified || vid_rgb24->modified || colorlight->modified) {
		vid_hires->modified = vid_beam->modified = vid_rgb24->modified = colorlight->modified = false;
		VID_HiresPicmip();
		vid_ref->modified = true;
	}
	if (!vid_ref->modified)
		return;
	vid_ref->modified = false;
	cl.force_refdef = true; /* can't use a paused refdef */
	S_StopAllSounds();
	cl.refresh_prepped = false;
	cls.disable_screen = true;
	re.Shutdown();
	if (re.Init(NULL, NULL) == -1)
		Com_Error(ERR_FATAL, "Couldn't start refresh");
	cls.disable_screen = false;
}

/* ---- the Video menu: the TV's mode or 1080i (vid_hires), beam racing
   (vid_beam), colour (vid_rgb24) ---------------------------------------- */

void M_PopMenu(void);

static menuframework_s vid_menu;
static menulist_s vid_mode_list, vid_beam_list, vid_rgb24_list, vid_light_list;
static menuaction_s vid_apply;

static void VID_ApplyCallback(void *unused) {
	Cvar_SetValue("vid_hires", vid_mode_list.curvalue);
	Cvar_SetValue("vid_beam", vid_beam_list.curvalue);
	Cvar_SetValue("vid_rgb24", vid_rgb24_list.curvalue);
	Cvar_SetValue("gl_ps2colorlight", vid_light_list.curvalue);
	M_ForceMenuOff();
}

static void VID_MenuList(menulist_s *l, const char *name, const char **items, int y, int value) {
	l->generic.type = MTYPE_SPINCONTROL;
	l->generic.name = name;
	l->generic.x = 0;
	l->generic.y = y;
	l->itemnames = items;
	l->curvalue = value;
	Menu_AddItem(&vid_menu, l);
}

void VID_MenuInit(void) {
	static const char *modes[] = {"TV (NTSC/PAL)", "1080i", 0};
	static const char *offon[] = {"off", "on", 0};
	static const char *colours[] = {"16-bit", "24-bit (TV, beam racing)", 0};
	vid_menu.x = viddef.width / 2;
	vid_menu.y = viddef.height / 2 - 30;
	vid_menu.nitems = 0;
	VID_MenuList(&vid_mode_list, "video mode", modes, 0, Cvar_VariableValue("vid_hires") == 1);
	VID_MenuList(&vid_beam_list, "beam racing", offon, 10, Cvar_VariableValue("vid_beam") != 0);
	VID_MenuList(&vid_rgb24_list, "colour", colours, 20, Cvar_VariableValue("vid_rgb24") != 0);
	/* coloured light: the lightmaps' pass three times (a channel each) and
	   their GS memory three times; with beam racing they are monochrome */
	static const char *lights[] = {"monochrome", "coloured", 0};
	VID_MenuList(&vid_light_list, "light", lights, 30, Cvar_VariableValue("gl_ps2colorlight") != 0);
	vid_apply.generic.type = MTYPE_ACTION;
	vid_apply.generic.name = "apply";
	vid_apply.generic.x = 0;
	vid_apply.generic.y = 50;
	vid_apply.generic.callback = VID_ApplyCallback;
	Menu_AddItem(&vid_menu, &vid_apply);
	Menu_SetStatusBar(&vid_menu, "beam racing: more textures, max 60 fps");
	Menu_Center(&vid_menu);
}

void VID_MenuDraw(void) {
	int w, h;
	vid_menu.x = viddef.width / 2;
	re.DrawGetPicSize(&w, &h, "m_banner_video");
	re.DrawPic(viddef.width / 2 - w / 2, viddef.height / 2 - 110, "m_banner_video");
	/* light: monochrome with beam racing (libgl draws it so) */
	if (vid_beam_list.curvalue)
		vid_light_list.generic.flags |= QMF_GRAYED;
	else
		vid_light_list.generic.flags &= ~QMF_GRAYED;
	Menu_AdjustCursor(&vid_menu, 1);
	Menu_Draw(&vid_menu);
}

const char *VID_MenuKey(int key) {
	menuframework_s *m = &vid_menu;
	switch (key) {
	case K_ESCAPE:
		M_PopMenu();
		return NULL;
	case K_UPARROW:
		m->cursor--;
		Menu_AdjustCursor(m, -1);
		break;
	case K_DOWNARROW:
		m->cursor++;
		Menu_AdjustCursor(m, 1);
		break;
	case K_LEFTARROW:
		Menu_SlideItem(m, -1);
		break;
	case K_RIGHTARROW:
		Menu_SlideItem(m, 1);
		break;
	case K_ENTER:
		Menu_SelectItem(m);
		break;
	}
	return "misc/menu1.wav";
}

/* ---- input: pad 0 ------------------------------------------------------- */

static int pad_ok;
static const struct {
	u32 bit;
	int key;
	int play; /* the key while playing, 0: the same (default.cfg: SPACE jumps,
	             c crouches, ENTER uses the item) */
} pad_keys[] = {
	{MX_PAD_UP, K_UPARROW},     {MX_PAD_DOWN, K_DOWNARROW}, {MX_PAD_LEFT, K_LEFTARROW},
	{MX_PAD_RIGHT, K_RIGHTARROW}, {MX_PAD_CROSS, K_ENTER, K_SPACE}, {MX_PAD_CIRCLE, K_AUX1, K_ENTER},
	{MX_PAD_SQUARE, K_AUX2},    {MX_PAD_TRIANGLE, K_AUX3},  {MX_PAD_R1, K_CTRL},
	{MX_PAD_R2, K_AUX4},        {MX_PAD_L1, K_AUX5},        {MX_PAD_L2, K_AUX6},
	{MX_PAD_START, K_ESCAPE},   {MX_PAD_SELECT, K_TAB},     {MX_PAD_R3, 'c'},
};
#define PAD_KEYS (sizeof pad_keys / sizeof pad_keys[0])
static float stick[4]; /* right x, y, left x, y: -1 .. 1 */
cvar_t *in_joystick; /* the options menu shows it */

void IN_Init(void) {
	in_joystick = Cvar_Get("in_joystick", "1", CVAR_ARCHIVE);
	pad_ok = mx_pad_init() >= 0;
	Com_Printf("pad: %s\n", pad_ok ? "ok" : "no driver");
}

void IN_Shutdown(void) {}
void IN_Commands(void) {}
void IN_Activate(qboolean active) {}

void Sys_SendKeyEvents(void) {
	if (pad_ok) {
		static u32 last;
		u32 held = mx_pad_buttons(0);
		unsigned t = Sys_Milliseconds();
		static int down[PAD_KEYS]; /* the key each button pressed: released the same */
		for (unsigned i = 0; i < PAD_KEYS; i++) {
			u32 b = pad_keys[i].bit;
			if (!((held ^ last) & b))
				continue;
			if (held & b)
				down[i] = cls.key_dest == key_game && pad_keys[i].play ? pad_keys[i].play : pad_keys[i].key;
			Key_Event(down[i], (held & b) != 0, t);
		}
		last = held;
		u8 st[4];
		if (mx_pad_sticks(0, st))
			for (int k = 0; k < 4; k++) {
				int v = (int)st[k] - 128;
				v = v > -32 && v < 32 ? 0 : v > 0 ? v - 32 : v + 32;
				stick[k] = v / 96.0f;
			}
	}
	sys_frame_time = Sys_Milliseconds();
}

void IN_Frame(void) {}

/* the left stick moves, the right one turns (180 degrees a second at full
   turn) */
void IN_Move(usercmd_t *cmd) {
	if (cls.key_dest != key_game)
		return;
	cmd->forwardmove -= stick[3] * cl_forwardspeed->value;
	cmd->sidemove += stick[2] * cl_sidespeed->value;
	cl.viewangles[YAW] -= stick[0] * 180.0f * cls.frametime;
	cl.viewangles[PITCH] += stick[1] * 135.0f * cls.frametime;
}
