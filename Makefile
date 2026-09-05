# Super Mario World — Retro-Go SD GWHB homebrew
#
#   make                        — build + pack Super Mario World.bin
#   make docker                 — same inside the builder image
#   make host                   — SDL2 desktop preview (smw_host)
#
# Sidecar on SD (not in the GWHB): /homebrews/smw_assets.dat
# Build assets from a SMW ROM via external/smw (see README).
# Verbose compiler lines: make V=

#######################################
# Project identity
#######################################
PROJECT_KIND ?= homebrew

CORE_NAME  := smw
CORE_ENTRY := app_main

CORE_SMW := external/smw

CORE_C_SOURCES := \
$(CORE_SMW)/src/smw_rtl.c \
$(CORE_SMW)/src/smw_00.c \
$(CORE_SMW)/src/smw_01.c \
$(CORE_SMW)/src/smw_02.c \
$(CORE_SMW)/src/smw_03.c \
$(CORE_SMW)/src/smw_04.c \
$(CORE_SMW)/src/smw_05.c \
$(CORE_SMW)/src/smw_07.c \
$(CORE_SMW)/src/smw_0c.c \
$(CORE_SMW)/src/smw_0d.c \
$(CORE_SMW)/src/smw_cpu_infra.c \
$(CORE_SMW)/src/smw_spc_player.c \
$(CORE_SMW)/src/config.c \
$(CORE_SMW)/src/common_rtl.c \
$(CORE_SMW)/src/common_cpu_infra.c \
$(CORE_SMW)/src/util.c \
$(CORE_SMW)/src/lm.c \
$(CORE_SMW)/src/snes/ppu.c \
$(CORE_SMW)/src/snes/dma.c \
$(CORE_SMW)/src/snes/dsp.c \
$(CORE_SMW)/src/snes/apu.c \
$(CORE_SMW)/src/snes/spc.c \
$(CORE_SMW)/src/snes/snes.c \
$(CORE_SMW)/src/snes/cpu.c \
$(CORE_SMW)/src/snes/cart.c \
$(CORE_SMW)/src/tracing.c \
src/main_smw.c

CORE_C_INCLUDES := \
-Isrc \
-I$(CORE_SMW) \
-Iexternal

# HEADLESS + defaults match firmware Makefile.common classic SMW build.
CORE_C_DEFS := \
-DPROJECT_KIND_HOMEBREW=1 \
-DHEADLESS \
-DLIMIT_30FPS=1 \
-DFEATURES=0

GNW_CORE_SDK ?= sdk
BUILD_DIR ?= build/$(PROJECT_KIND)

PACKED_BIN := Super Mario World.bin
HB_NAME    := Super Mario World
COVER_JPG  := $(BUILD_DIR)/cover.jpg
COVER_SRC  := src/assets/cover_src.png

include $(GNW_CORE_SDK)/Makefile

PACK_HOMEBREW := $(GNW_CORE_SDK)/tools/pack_homebrew.py

# Upstream warn suppressions (mirrors Makefile.common smw_obj_prereq_gen).
SMW_WARN_OFF := -Wno-parentheses -Wno-unknown-pragmas -Wno-unused-variable \
	-Wno-unused-but-set-variable -Wno-unused-const-variable -Wno-int-in-bool-context \
	-Wno-unused-value -Wno-unused-function -Wno-incompatible-pointer-types \
	-Wno-implicit-function-declaration -Wno-format -Wno-array-bounds \
	-Wno-strict-aliasing -Wno-maybe-uninitialized
CFLAGS += $(SMW_WARN_OFF) -std=gnu11
ASFLAGS += -std=gnu11

#######################################
# Packed header version
#######################################
CORE_VERSION ?= $(shell git describe --tags --dirty 2>/dev/null || echo NOTAG)

#######################################
# Pack
#######################################
.PHONY: pack cover

cover: $(COVER_JPG)

# Must stay ≤ gui.c COVER_MAX (186×100) and COVER_SIZE (10 KiB).
$(COVER_JPG): $(COVER_SRC)
	$(V)$(ECHO) [ COVER ] $(COVER_JPG)
	$(V)mkdir -p $(BUILD_DIR)
	$(V)python3 -c "from pathlib import Path; from PIL import Image; \
img=Image.open('$(COVER_SRC)').convert('RGB'); \
img.thumbnail((186,100)); \
img.save('$(COVER_JPG)', 'JPEG', quality=85, optimize=True); \
sz=Path('$(COVER_JPG)').stat().st_size; \
assert sz <= 10*1024, f'cover too big: {sz}'; \
w,h=img.size; assert w<=186 and h<=100, (w,h)"

pack: $(TARGET_BIN) $(COVER_JPG)
	$(V)$(ECHO) [ PACK GWHB ] "$(PACKED_BIN)" version=$(CORE_VERSION)
	$(V)python3 $(PACK_HOMEBREW) \
		--elf $(TARGET_ELF) --bin $(TARGET_BIN) \
		--name "$(HB_NAME)" --version "$(CORE_VERSION)" \
		--cover $(COVER_JPG) \
		--out "$(PACKED_BIN)"

all: pack

.PHONY: print-PROJECT_KIND print-PACKED_BIN print-CORE_NAME print-DOCKER_IMAGE \
	print-TARGET_ELF print-TARGET_MAP print-CORE_VERSION
print-PROJECT_KIND:
	@echo $(PROJECT_KIND)
print-PACKED_BIN:
	@echo $(PACKED_BIN)
print-CORE_NAME:
	@echo $(CORE_NAME)
print-DOCKER_IMAGE:
	@echo $(DOCKER_IMAGE)
print-TARGET_ELF:
	@echo $(TARGET_ELF)
print-TARGET_MAP:
	@echo $(BUILD_DIR)/$(CORE_NAME)_core.map
print-CORE_VERSION:
	@echo $(CORE_VERSION)

clean::
	$(V)rm -f "$(PACKED_BIN)" $(COVER_JPG)

#######################################
# Docker (same image as firmware repo)
#######################################
.PHONY: docker docker_pull docker_shell

RELEASE_VERSION ?= v1.5
DOCKER_REPOSITORY ?= sylverb/retro-go-sd-builder
DOCKER_IMAGE ?= $(DOCKER_REPOSITORY):$(RELEASE_VERSION)

DOCKER_TTY_FLAG := $(shell if [ -t 0 ]; then echo -it; else echo; fi)
DOCKER_USER := $(shell id -u):$(shell id -g)
DOCKER_RUN := docker run --rm $(DOCKER_TTY_FLAG) \
	--user $(DOCKER_USER) \
	-v "$(CURDIR):/opt/workdir" \
	-w /opt/workdir \
	$(DOCKER_IMAGE)

docker:
	$(V)$(ECHO) "[ DOCKER ]" $(DOCKER_IMAGE) "PROJECT_KIND=$(PROJECT_KIND)"
	$(V)$(DOCKER_RUN) make --no-print-directory -j$$(nproc) PROJECT_KIND=$(PROJECT_KIND)

docker_pull:
	$(V)$(ECHO) "[ PULL ]" $(DOCKER_IMAGE)
	$(V)docker pull $(DOCKER_IMAGE)

docker_shell:
	$(DOCKER_RUN) bash

#######################################
# Host SDL (Linux / macOS)
#######################################
include host/Makefile.host
