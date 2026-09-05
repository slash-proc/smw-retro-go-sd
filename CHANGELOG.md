# Changelog

This file follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
[Semantic Versioning](https://semver.org/spec/v2.0.0.html). A release tag must
match a section heading exactly (for example `v1.0.0`): CI reads the matching
section and uses it as the GitHub Release notes, and refuses to release without
one.

When you cut a release:

1. Move items from `[Unreleased]` into a new `## [vX.Y.Z] - YYYY-MM-DD` section.
2. Commit the changelog update.
3. Push the tag: `git tag vX.Y.Z && git push origin vX.Y.Z`

## [Unreleased]

## [v0.2.0] - 2026-09-05

### Changed

- Whether an unrecognised ROM may be used is now the host's decision, taken
  before the run, from the manifest's new `inputs[].strict` field. This
  project's base ROM is `strict: false`, because a Lunar Magic hack cannot
  match a known hash by construction.
- The extractor no longer refuses a ROM it does not recognise. It converts it
  and says so through `warnings`, leaving admission to the one party that has
  the file, the hashes and the user in front of it.
- The conversion page enforces `strict` itself: it hashes each file, refuses a
  stranger for a strict input, and warns for one that is not.

### Removed

- Flag bit 0, `noHashCheck`. Its job is now `strict`, and a caller still
  setting it gets an error rather than a run that means something else.
- Both `options[]` entries. `noHashCheck` is replaced; `noIncludeRom` is not
  something this project wants offered, so the manifest declares `"options":
  []` and the ROM data is always included.

## [v0.1.0] - 2026-09-05

### Added

- Super Mario World as a standalone GWHB homebrew, packed as
  `Super Mario World.bin`.
- The asset extractor, vendored from `slash-proc/smw` into `tools/extractor/`:
  a zero-import WASM module that turns the user's own ROM into
  `smw_assets.dat`.
- Publishing under the [GWRG distribution
  spec](https://github.com/slash-proc/gwrg-dist-spec): a `manifest.json`
  declaring both the binary and the extractor, an offline bundle, and a
  GitHub Pages mirror of `dist/` that a web installer can read.
