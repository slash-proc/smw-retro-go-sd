# Super Mario World for Game & Watch Retro-Go SD

Standalone **GWHB homebrew** port of [smw](https://github.com/sylverb/smw)
(filesystem branch), packaged with this repo's Retro-Go SD core SDK.

## Build

```bash
git submodule update --init --recursive
make                 # → Super Mario World.bin
make docker          # same, inside sylverb/retro-go-sd-builder
make host            # → smw_host (SDL2 desktop preview)
```

Requires `arm-none-eabi-gcc` (hard-float `fpv5-d16`) and
`pip install -r requirements.txt` (Pillow for the cover). Host also needs
SDL2 (`brew install sdl2` / `libsdl2-dev`).

## Host preview

```bash
mkdir -p homebrews
cp /path/to/smw_assets.dat homebrews/
# or: HOST_SD=/path/to/sdcard  (expects $HOST_SD/homebrews/smw_assets.dat)
./smw_host
```

`HOST_OFW_MARIO=1` selects the Mario OFW face-button layout (default = Zelda).

## Install on SD

1. Copy `Super Mario World.bin` to `/homebrews/Super Mario World.bin`.
2. Build and copy the asset sidecar (not packed into the GWHB):

```bash
# Place a Super Mario World ROM as external/smw/assets/smw.sfc
make -C external/smw smw_assets.dat
cp external/smw/smw_assets.dat /path/to/sd/homebrews/smw_assets.dat
```

The homebrew loads `/homebrews/smw_assets.dat` via
`odroid_overlay_cache_file_in_flash`. An optional cover override can live at
`/covers/homebrew/Super Mario World.img`.

## Layout

| Path | Role |
|------|------|
| `external/smw/` | Engine submodule (HEADLESS build) |
| `src/main_smw.c` | G&W platform (LCD, audio, input, save/SRAM) |
| `src/smw_borders.h` | Side border 1bpp art |
| `sdk/` | Vendored ABI bridge, headers, packer |

## Notes

- Default build is **30 FPS** (`LIMIT_30FPS=1`) with audio rendered for two
  frames per display frame (same as the in-firmware SMW homebrew).
- SNES VRAM is allocated in **ITCM** (`itc_calloc`) — the full 64 KiB budget.
- Button bindings follow the Mario vs Zelda OFW face layout
  (`get_ofw_is_mario`).
