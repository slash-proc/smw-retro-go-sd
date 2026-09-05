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

## [v0.3.0] - 2026-09-06

### Added

- A version picker. The page reads this site's own `dist/versions.json`,
  offers every version the mirror holds and defaults to the newest, so a
  release reaches users without redeploying the page. Switching version
  reloads the module, its inputs and its accepted hashes, and discards any
  file already chosen: a file one version accepts another may refuse, and
  carrying it over would start a run on input this version never approved.
  Pre-releases are hidden until asked for, and each version shows the
  firmware ABI it needs, which is the only place a user learns it.
- A single download containing the whole install: the artifacts the project
  published plus the file the run just produced, as
  `smw-<tag>-gwrg.zip`. The converted file on its own was only half of what
  somebody needs, and the manifest already named and hashed the other half.
  Published files are fetched, checked against the size and hash the
  manifest declares, and a mismatch refuses the zip rather than shipping a
  broken install.
- `zip.mjs`, a dependency-free zip writer. Entries are deflated where that
  helps and stored where it does not; a browser without `CompressionStream`
  stores everything and still produces a valid archive.
- `test-i18n.mjs`, which demands every string the page asks for from every
  locale it offers. A renamed key used to be invisible: the page still
  loads and the control is simply blank for that language.

### Changed

- The page is now byte-identical to the one zelda3 ships, and carries no
  fact about any particular game. `smw_assets.dat` was written into the
  privacy note in all three languages; it comes from the manifest now.
  The page also gained repeatable inputs, per-role help, refusals that say
  what a file actually hashed to, and a picker that notices when the same
  file is chosen twice.
- `outputs[].maxBytes` is enforced. The manifest states a ceiling per output
  as well as one for the whole run, and only the second was being applied.
- `config.json` points at the version index rather than one manifest.
  `MANIFEST_URL` still pins a build to a single manifest, which is what an
  offline bundle needs.

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

## [v0.0.1]

Initial release of the upstream project,
[sylverb/smw-retro-go-sd](https://github.com/sylverb/smw-retro-go-sd), as a
homebrew for Game & Watch Retro-Go SD. This fork's own releases start at
v0.1.0.
