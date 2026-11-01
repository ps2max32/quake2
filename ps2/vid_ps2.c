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

/* vid_hires 1: 1080i (glimp_ps2.c), with the textures at half size
   (gl_picmip 1): a room's do not fit in GS memory at full size (1.8 MiB
   even as 8-bit indices), with 1080i's buffer in it */
static cvar_t *vid_ref, *vid_hires;
static void VID_HiresPicmip(void) {
	Cvar_SetValue("gl_picmip", vid_hires->value == 1);
}

static void VID_Restart_f(void) { vid_ref->modified = true; }

void VID_Init(void) {
	refimport_t ri;
	vid_ref = Cvar_Get("vid_ref", "gl", 0);
	vid_hires = Cvar_Get("vid_hires", "0", 0);
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
	vid_hires->modified = false;
}

void VID_Shutdown(void) {
	if (re.Shutdown)
		re.Shutdown();
}

/* the refresh again (win32/vid_dll.c's VID_CheckChanges): the mode is set
   as it starts, the textures and models are loaded again (gl_picmip) */
void VID_CheckChanges(void) {
	if (vid_hires->modified) {
		vid_hires->modified = false;
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

/* ---- the Video menu: the TV's mode or 1080i (vid_hires) --------------------- */

void M_PopMenu(void);

static menuframework_s vid_menu;
static menulist_s vid_mode_list;
static menuaction_s vid_apply;

static void VID_ApplyCallback(void *unused) {
	Cvar_SetValue("vid_hires", vid_mode_list.curvalue);
	M_ForceMenuOff();
}

void VID_MenuInit(void) {
	static const char *modes[] = {"TV (NTSC/PAL)", "1080i", 0};
	vid_menu.x = viddef.width / 2;
	vid_menu.y = viddef.height / 2 - 30;
	vid_menu.nitems = 0;
	vid_mode_list.generic.type = MTYPE_SPINCONTROL;
	vid_mode_list.generic.name = "video mode";
	vid_mode_list.generic.x = 0;
	vid_mode_list.generic.y = 0;
	vid_mode_list.itemnames = modes;
	vid_mode_list.curvalue = Cvar_VariableValue("vid_hires") == 1;
	vid_apply.generic.type = MTYPE_ACTION;
	vid_apply.generic.name = "apply";
	vid_apply.generic.x = 0;
	vid_apply.generic.y = 20;
	vid_apply.generic.callback = VID_ApplyCallback;
	Menu_AddItem(&vid_menu, &vid_mode_list);
	Menu_AddItem(&vid_menu, &vid_apply);
	Menu_SetStatusBar(&vid_menu, "1080i needs a TV that takes it (component)");
	Menu_Center(&vid_menu);
}

void VID_MenuDraw(void) {
	int w, h;
	vid_menu.x = viddef.width / 2;
	re.DrawGetPicSize(&w, &h, "m_banner_video");
	re.DrawPic(viddef.width / 2 - w / 2, viddef.height / 2 - 110, "m_banner_video");
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
} pad_keys[] = {
	{MX_PAD_UP, K_UPARROW},     {MX_PAD_DOWN, K_DOWNARROW}, {MX_PAD_LEFT, K_LEFTARROW},
	{MX_PAD_RIGHT, K_RIGHTARROW}, {MX_PAD_CROSS, K_ENTER},  {MX_PAD_CIRCLE, K_AUX1},
	{MX_PAD_SQUARE, K_AUX2},    {MX_PAD_TRIANGLE, K_AUX3},  {MX_PAD_R1, K_CTRL},
	{MX_PAD_R2, K_AUX4},        {MX_PAD_L1, K_AUX5},        {MX_PAD_L2, K_AUX6},
	{MX_PAD_START, K_ESCAPE},   {MX_PAD_SELECT, K_TAB},
};
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
		for (unsigned i = 0; i < sizeof pad_keys / sizeof pad_keys[0]; i++) {
			u32 b = pad_keys[i].bit;
			if ((held ^ last) & b)
				Key_Event(pad_keys[i].key, (held & b) != 0, t);
		}
		/* R3 (the right stick pressed): the counters on and off */
		if (held & ~last & MX_PAD_R3)
			mxgl_show_stats = !mxgl_show_stats;
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
