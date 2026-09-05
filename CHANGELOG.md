# Changelog

## [v0.0.1]
Initial release as a homebrew for Game-And-Watch-Retro-Go-SD

### Added

- Nothing

### Changed

- Nothing

### Fixed

- Nothing

### Install

Unzip the release archive onto the SD card root. It already contains:

- `/homebrews/Super Mario World.bin` — GWHB homebrew

You still need the game assets (not shipped in the zip — extract from your own
ROM):

```bash
# Place a Super Mario World ROM as external/smw/assets/smw.sfc
make -C external/smw smw_assets.dat
cp external/smw/smw_assets.dat /path/to/sd/homebrews/smw_assets.dat
```

Optional coverflow override: `/covers/homebrew/Super Mario World.img`
(JPEG ≤186×100, ≤10 KiB). Firmware ABI must match `SDK_VERSION` in this
repository.
