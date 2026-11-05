# Quake 2 on maxsdk (PS2)

id's Quake 2 (GPL) built for the PS2 with `libs/maxsdk` (clang, R5900) by `Makefile`. The
port is in `ps2/`; the engine changes are under `#ifdef __PS2__` or marked, and few:
`gl_rsurf.c` (lightmaps as brightness
textures, 2x), `gl_rmain.c` (`R_Init`'s missing `return 0`), `q_shared.c` (no
`double sqrt`), `g_items.c` (`static` that clang refuses), `snd_mem.c` (a sound's memory
sized as it is stored: 8-bit with `s_loadas8bit`), `snd_mix.c` (the C mixer's 8-bit volume
table `>> 3`, as the x86 assembly: id's `>> 11` mixed every 8-bit sound silent). id's `makefile` (Rhapsody) is
`makefile.rhapsody`, so that `make` finds `Makefile`.

```
make [FRAMES=n] [ARGS="+map base1"]    $(OUT)/quake2.elf (build/apps/quake2)
make run                               copies the retail baseq2/pak0..2 (Q2DATA) to
                                       build/pcsx2-test/baseq2 and runs it in PCSX2
```

`FRAMES=n` ends the program after n drawn frames with a picture (`host:quake2.tga`) and
the test end marker; `ARGS` is added to the command line.

| Part | How |
|---|---|
| Refresh | `ref_gl` on `libs/libgl`: `ps2/qgl_ps2.c` (the `qgl*` pointers, made from `qgl.h`), `ps2/glimp_ps2.c`; lightmaps in colour (the Video menu's light, `gl_ps2colorlight 1`, the default: libgl's `mxgl_color_light` draws their pass a channel at a time, three passes; with beam racing, or 0, brightness textures the GS multiplies by, `Cd * As`, up to 2x: monochrome); the textures at full size in every mode, their top levels left out only while GS memory overflows or the swap file is read (`gl_ps2texquality -1`, libgl's `MXGL_TEX_AUTO`; 0, 1, 2: that many levels always; `gl_ps2texlimits 120,80,30`) |
| Modules | `game/` and `ref_gl/` each linked into one object, symbols local but `GetGameAPI` / `GetRefAPI` (`game_` / `ref_` in front); `Sys_GetGameAPI` and `VID_Init` call them |
| Memory | maxsdk's virtual memory (`mx_vm_config` in `ps2/sys_ps2.c`): `malloc` in up to 128 MiB, paged to `host:quake2.swp`; `Hunk_Begin` takes what it is asked for (pages not used are not touched, as Linux's `mmap`), `Hunk_End` gives the rest back (realloc in place); built into VM (`MX_MEMORY=vm`, the default: started by `mxload.elf`, as `make run` does), only maxsdk, libc and libgl in physical memory; libgl's pinned memory (what the GS reads by DMA) taken from the frames as needed, the textures' larger levels pageable; stack 1 MiB. In PCSX2 only on the EE interpreter (`PCSX2_EE=interp`) |
| Files | stdio on the IOP file server (`host:`), `ps2/ps2_file.c`; `+set basedir host:` |
| Sound | id's mixer (`client/snd_dma.c`, `snd_mix.c`) at `s_khz` (11025 Hz by default: the sounds are kept in memory at that rate) 16-bit stereo into a ring, sent to maxsdk's PCM stream (`mx/snd.h`, audsrv) by `ps2/snddma_ps2.c` as far as the mixer has painted |
| Network | loopback only (`ps2/net_ps2.c`) |
| Input | pad 0: d-pad arrows, left stick moves, right stick turns, R1 attack (CTRL), X jump (Enter in menus), R3 crouch, O use item, START menu, L3 + R3 (both sticks pressed) the counters on/off, SELECT TAB; O, square, triangle, R2, L1, L2 are AUX1..6 (bind them) |

Not done: CD audio (null driver), cinematics are decoded but not tested,
`Sys_FindFirst` (no save game list), network play. The textures of a room do not all fit
in GS memory: they are copied in again each frame (`gl_picmip 1` avoids it).

## Video modes: 1080i, beam racing, 24-bit colour

The Video menu sets the mode (the TV's or 1080i: `vid_hires 0` / `1`, not kept in the
config), beam racing (`vid_beam`) and the colour (`vid_rgb24`); apply starts the refresh
again (`VID_CheckChanges`, as id's `vid_restart`): the mode is set as it starts
(`ps2/glimp_ps2.c`), and its textures and models are loaded again.

- 1080i: the game's window is 960 x 540 (its 2D twice as large), drawn on the 1920 x 1080
  screen. A room's textures do not all fit in its GS memory at full size (1.8 MiB even as
  8-bit): libgl leaves their top level out while they refill (`MXGL_TEX_AUTO`), no longer
  `gl_picmip 1` for all of them. Without beam racing the picture is 640 x 540 stretched to
  the screen.
- Beam racing (libgl's `MXGL_BEAM`): one buffer of a field's lines, drawn in strips just
  ahead of the beam every field; at most a frame a field (60, PAL 50). In the TV's mode it
  leaves 3.6 MiB of GS memory for textures (without: 1.8, PAL 1.5), so a room's fit at
  full size. It takes 2 MiB more of pinned memory while it is on.
- 24-bit colour (`MXGL_RGB24`): with beam racing in the TV's mode (3.3 MiB for textures).

On the PS2 (2026-10-31, the 1080i-only build before the switch): `demo1.dm2` at 60 to 62
fps, a few seconds of the fight at 55 to 58; no pass late. World polygons go through
`mxgl_DrawPolygon`, opaque models through `mxgl_DrawGlCmds`, translucent ones through
`mxgl_MultiDraw` (`ref_gl/gl_mesh.c`), particles as one vertex array, the BSP walk passes
the frustum planes down; `GL_FindImage` has a name cache, the palette goes to libgl
after the intensity and gamma tables (`ref_gl/gl_image.c`). `make PROF=1` profiles the
second half of a `FRAMES` run; `PROF=<t>` (ms) the game time t .. t + 1 s (a demo's, the
same in every run; `ps2: frame` lines show it), and ends there.
