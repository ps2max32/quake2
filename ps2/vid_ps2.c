/* vid_ps2.c: the video and input of Quake 2 on maxsdk: the GL refresh
   (ref_gl, linked in: ref_GetRefAPI) on maxsdk_gl, and pad 0. From
   null/vid_null.c. */

#include <mx/mx.h>
#include <mx/pad.h>

#include "../client/client.h"

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

/* one mode: maxsdk_gl's 640 x 448 */
qboolean VID_GetModeInfo(int *width, int *height, int mode) {
	*width = 640;
	*height = 448;
	return true;
}

void VID_Init(void) {
	refimport_t ri;
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
}

void VID_Shutdown(void) {
	if (re.Shutdown)
		re.Shutdown();
}

void VID_CheckChanges(void) {}
void VID_MenuInit(void) {}
void VID_MenuDraw(void) {}
const char *VID_MenuKey(int k) { return NULL; }

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
