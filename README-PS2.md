# Quake 2 on maxsdk (PS2)

id's Quake 2 (GPL) built for the PS2 with `libs/maxsdk` (clang, R5900) by `Makefile`. The
port is in `ps2/`; the engine changes are under `#ifdef __PS2__` or marked, and few:
`cmodel.c` (smaller collision-map arrays), `gl_rsurf.c` (lightmaps as brightness
textures, 2x), `gl_rmain.c` (`R_Init`'s missing `return 0`), `q_shared.c` (no
`double sqrt`), `g_items.c` (`static` that clang refuses). id's `makefile` (Rhapsody) is
`makefile.rhapsody`, so that `make` finds `Makefile`.

```
make [FRAMES=n] [ARGS="+map base1"]    $(OUT)/quake2.elf (build/apps/maxsdk_quake2)
make run                               copies the retail baseq2/pak0..2 (Q2DATA) to
                                       build/pcsx2-test/baseq2 and runs it in PCSX2
```

`FRAMES=n` ends the program after n drawn frames with a picture (`host:quake2.tga`) and
the test end marker; `ARGS` is added to the command line.

| Part | How |
|---|---|
| Refresh | `ref_gl` on `libs/maxsdk_gl`: `ps2/qgl_ps2.c` (the `qgl*` pointers, made from `qgl.h`), `ps2/glimp_ps2.c`; lightmaps are brightness textures the GS multiplies by (`Cd * As`), up to 2x: monochrome |
| Modules | `game/` and `ref_gl/` each linked into one object, symbols local but `GetGameAPI` / `GetRefAPI` (`game_` / `ref_` in front); `Sys_GetGameAPI` and `VID_Init` call them |
| Memory | `Hunk_Begin` takes what is free less 3 MiB, `Hunk_End` gives the rest back (picolibc's `realloc` shrinks in place); stack 1 MiB |
| Files | stdio on the IOP file server (`host:`), `ps2/ps2_file.c`; `+set basedir host:` |
| Network | loopback only (`ps2/net_ps2.c`) |
| Input | pad 0: d-pad arrows, left stick moves, right stick turns, R1 attack (CTRL), X enter, START menu, SELECT TAB; O, square, triangle, R2, L1, L2 are AUX1..6 (bind them) |

Not done: sound and CD audio (null drivers), cinematics are decoded but not tested,
`Sys_FindFirst` (no save game list), network play. The textures of a room do not all fit
in GS memory: they are copied in again each frame (`gl_picmip 1` avoids it).

## Branch `hires`: 1080i

This branch builds the 1920 x 1080 interlaced version (`$(OUT)` is
`build/apps/maxsdk_quake2-hires`): maxsdk_gl with `MXGL_HIRES=1`, on the maxsdk worktree of
branch `hires` (`libs/maxsdk-hires`, built into `build/libs/maxsdk-hires`). The game's
window is 960 x 540 (its 2D twice as large), drawn at 1920 x 1080; textures at half size
(`gl_picmip 1`) so a room's fit in GS memory, as each pass uploads the frame's textures again.
