# Quake 2 (id's source) on maxsdk: the PS2 port.
#
#   make [OUT=<dir>] [FRAMES=n] [ARGS="+map base1"]   $(OUT)/quake2.elf
#   make run             quake2.elf in PCSX2 with the game data in
#                        build/pcsx2-test/baseq2 (Q2DATA: the retail
#                        baseq2 directory, copied there once)
#
# The engine (client, server, qcommon) is linked with the game (game/) and
# the GL refresh (ref_gl/) built natively: each of these two is linked into
# one object whose symbols are local except GetGameAPI / GetRefAPI, renamed
# game_GetGameAPI / ref_GetRefAPI (their copies of q_shared.c, Com_Printf
# and Sys_Error do not clash). The refresh draws through libs/libgl
# (ps2/qgl_ps2.c, ps2/glimp_ps2.c). Sound is id's mixer on maxsdk's
# mx/snd.h stream (ps2/snddma_ps2.c); CD audio is id's null driver, the
# network the loopback and UDP by BSD sockets (ps2/net_ps2.c).

HERE     := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
PS2MAX   ?= $(abspath $(HERE)/../..)
MAXSDK   ?= $(PS2MAX)/libs/maxsdk
MXGL     ?= $(PS2MAX)/libs/libgl
OUT      ?= $(PS2MAX)/build/apps/quake2
all: $(OUT)/quake2.elf
include $(MXGL)/mk/mxgl.mk
# the program in VM (ld/maxsdk-vm.ld), started by mxlink; MX_MEMORY=phys:
# all of it in physical memory (32 MiB, no VM in PCSX2's recompiler)
MX_MEMORY ?= vm
include $(MAXSDK)/mk/maxsdk.mk
# in VM: Quake 2 in VM, libgl in core
MX_CORE_OBJS = $(MXGL_OBJS)

PICOLIBC ?= $(PS2MAX)/build/libs/picolibc/install
LIBC_LIBS = $(MAXSDK_OUT)/libc/glue.c.o $(MAXSDK_OUT)/libmxc.a $(PICOLIBC)/lib/libc.a \
            $(MX_LIB) $(MAXSDK_OUT)/librt.a

P        := $(HERE)/ps2
# 1997 C (gnu89: implicit declarations, common symbols); unsuffixed
# literals and the math.h functions are float (ps2/ps2_math.h): the FPU
# has no doubles
Q2CFLAGS  = $(filter-out -fno-common -ffreestanding -Wall -Wextra -Wno-unused-parameter -std=gnu11,$(CFLAGS)) \
            -std=gnu89 -w -Wno-error=incompatible-pointer-types -Wno-error=int-conversion -Wno-error=implicit-int -Wno-error=return-type -MMD -fcommon -fno-strict-aliasing -ffast-math -fapprox-func \
            -cl-single-precision-constant -nostdlibinc -isystem $(PICOLIBC)/include \
            $(MX_NET_CFLAGS) -D__PS2__ -DGL_GLEXT_PROTOTYPES -Dstricmp=strcasecmp -I$(P) -I$(OUT)/ps2 -I$(MXGL_INC) $(MXGL_DEFS) \
            -include $(P)/ps2_stdio.h -include $(P)/ps2_math.h

ENGINE   := client/cl_cin client/cl_ents client/cl_fx client/cl_input client/cl_inv \
            client/cl_main client/cl_newfx client/cl_parse client/cl_pred client/cl_tent client/cl_scrn \
            client/cl_view client/console client/keys client/menu client/snd_dma \
            client/snd_mem client/snd_mix client/qmenu game/m_flash \
            qcommon/cmd qcommon/cmodel qcommon/common qcommon/crc qcommon/cvar qcommon/files \
            qcommon/md4 qcommon/net_chan qcommon/pmove \
            server/sv_ccmds server/sv_ents server/sv_game server/sv_init server/sv_main \
            server/sv_send server/sv_user server/sv_world \
            game/q_shared null/cd_null ps2/snddma_ps2 \
            ps2/sys_ps2 ps2/vid_ps2 ps2/net_ps2 ps2/ps2_file
GAME     := $(basename $(notdir $(wildcard $(HERE)/game/*.c)))
REF      := $(addprefix ref_gl/,gl_draw gl_image gl_light gl_mesh gl_model gl_rmain gl_rmisc \
            gl_rsurf gl_warp) game/q_shared ps2/qgl_ps2 ps2/glimp_ps2

ENGINE_OBJS := $(patsubst %,$(OUT)/engine/%.o,$(ENGINE))

$(OUT)/engine/%.o: $(HERE)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(Q2CFLAGS) -c $< -o $@
$(OUT)/game/%.o: $(HERE)/game/%.c
	@mkdir -p $(dir $@)
	$(CC) $(Q2CFLAGS) -fno-common -c $< -o $@
$(OUT)/ref/%.o: $(HERE)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(Q2CFLAGS) -fno-common -c $< -o $@

# $(call module,out,objs,entry,prefix): one object, its symbols local but
# entry, renamed prefix_entry (built -fno-common: a common symbol made
# local is not given room)
define module
$(1): $(2)
	$(LLVM)/ld.lld -m elf32ltsmipn32 -r -o $$@.r $$^
	$(LLVM)/llvm-objcopy --keep-global-symbol=$(3) $$@.r $$@.l
	$(LLVM)/llvm-objcopy --redefine-sym=$(3)=$(4)_$(3) $$@.l $$@
	rm -f $$@.r $$@.l
endef
$(eval $(call module,$(OUT)/game.o,$(patsubst %,$(OUT)/game/%.o,$(GAME)),GetGameAPI,game))
$(eval $(call module,$(OUT)/ref.o,$(patsubst %,$(OUT)/ref/%.o,$(REF)),GetRefAPI,ref))
MODULES  := $(OUT)/game.o $(OUT)/ref.o

# FRAMES: end after that many frames with a picture (host:quake2.tga);
# ARGS: more of the command line. In $(OUT)/ps2/runcfg.h, rewritten only
# when they change.
RUNCFG := $(OUT)/ps2/runcfg.h
$(shell mkdir -p $(OUT)/ps2; printf '#define PS2_FRAMES %s\n#define PS2_ARGS "%s"\n#define PS2_PROF %s\n' \
  '$(or $(FRAMES),0)' '$(ARGS)' '$(or $(PROF),0)' > $(RUNCFG).new; \
  cmp -s $(RUNCFG).new $(RUNCFG) || mv $(RUNCFG).new $(RUNCFG); rm -f $(RUNCFG).new)

$(OUT)/quake2.elf: $(ENGINE_OBJS) $(MODULES) $(MXGL_OBJS) $(HLSL_OBJS) $(MX_LIB) $(MX_CRT0) $(MX_LD) $(LIBC_LIBS)
	$(call link,$@,--defsym=__mx_stack_size=0x100000 $(MX_CRT0) $(ENGINE_OBJS) $(MODULES) $(MXGL_OBJS) $(HLSL_OBJS) $(LIBC_LIBS))

# ---- running --------------------------------------------------------------------
Q2DATA   ?= /mnt/truenas_data_exthdd/spellen/PC/Quake II/baseq2
RUNDIR   := $(PS2MAX)/build/pcsx2-test
run: $(OUT)/quake2.elf
	@mkdir -p $(RUNDIR)/baseq2
	@for f in pak0 pak1 pak2; do [ -f $(RUNDIR)/baseq2/$$f.pak ] || \
	  cp -v "$(Q2DATA)/$$f.pak" $(RUNDIR)/baseq2/; done
ifeq ($(MX_MEMORY),vm)
	cp $(OUT)/quake2.elf $(RUNDIR)/quake2.elf && echo "host:quake2.elf" > $(RUNDIR)/mxload.txt
	PCSX2_EE=interp $(PCSX2_RUN) $(MX_LOADER) $(or $(TIMEOUT),1800)
else
	$(PCSX2_RUN) $(OUT)/quake2.elf $(or $(TIMEOUT),600)
endif

clean:
	rm -rf $(OUT)

.PHONY: all run clean

-include $(shell find $(OUT) -name '*.d' 2>/dev/null)
