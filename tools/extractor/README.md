# SMW asset extraction as a verifiable wasm module

A rewrite of `reference/restool.py` + `compile_resources.py` + `util.py` as a
single wasm module that takes a Super Mario World ROM and returns
`smw_assets.dat`. Output is byte-for-byte identical to the Python.

    ROM (in linear memory)  ->  [ wasm module ]  ->  smw_assets.dat

The point is not "Python in a browser". The point is that a stranger's web tool
can fetch this module and run it on a user's proprietary ROM *without trusting
this repository*, because the module's inability to do anything except
transform bytes is checkable from the binary. It **imports nothing at all** —
no filesystem, no network, no clock, no randomness, no JS bridge.

The machinery here is deliberately project-independent: everything except
`src/extract.rs` and `PROJECT.md` ports to another game unchanged.

The portable contract it implements now lives in the
[GWRG distribution spec](https://github.com/slash-proc/gwrg-dist-spec/blob/main/README.md), which is where the module ABI, the
host requirements and the publishing model are defined. This directory holds
the SMW implementation of it, plus the checks that gate a release.

## Where things are

| | |
|---|---|
| [`PROJECT.md`](PROJECT.md) | SMW specifics: accepted ROMs, output, flags, coverage |
| [`docs/verification.md`](docs/verification.md) | what `verify.mjs` checks, and why each check |
| [spec/04-processor.md](https://github.com/slash-proc/gwrg-dist-spec/blob/main/spec/04-processor.md) | the module contract: exports, stages, versioning |
| [spec/05-host.md](https://github.com/slash-proc/gwrg-dist-spec/blob/main/spec/05-host.md) | requirements for a tool that runs modules |
| [spec/03-manifest.md](https://github.com/slash-proc/gwrg-dist-spec/blob/main/spec/03-manifest.md) | how a module is declared to an installer |
| [spec/01-distribution.md](https://github.com/slash-proc/gwrg-dist-spec/blob/main/spec/01-distribution.md) | how a web tool finds it — and why not from releases |

## Files

| | |
|---|---|
| `src/rom.rs` | ROM addressing and the LZ decompressor (port of `util.py`) |
| `src/codec.rs` | RLE, RATS, stripe/level/exanim length probes |
| `src/pack.rs` | `pack_arrays`, `pack_blob`, container serialisation |
| `src/extract.rs` | port of `compile_resources.print_all`, as named stages |
| `src/lib.rs` | the wasm ABI |
| `verify.mjs` | conformance verifier — dependency-free, browser or node |
| `extract.mjs` | host runner (verify + instantiate + drive) |
| `manifest.mjs` | release manifest generator |
| `record-reference.mjs` | records verified output hashes, run only by `check.sh` |
| `test.mjs` | verifier tests: non-conformant modules that must be rejected |
| `test-abi.mjs` | ABI behaviour: errors, flags, cancellation, stepped/one-shot parity |
| `test-page.mjs` | drives the published page in a real browser |
| `page/` | the manual conversion site ([`page/README.md`](page/README.md)) |
| `build-page.sh` | assembles `site/` from the page and the built module |

## Build and test

```console
$ rustup target add wasm32-unknown-unknown
$ ./check.sh                      # build, verifier tests, conformance
$ ./check.sh /path/to/smw.sfc     # also parity against the Python, and ABI tests
```

`check.sh` uses whichever `cargo` is first on `PATH`; with both a distro rustc
and rustup installed, make sure `~/.cargo/bin` comes first or the wasm target
will appear to be missing.

The page:

```console
$ ./build-page.sh && (cd site && python3 -m http.server 8731)
$ node test-page.mjs                 # load-only checks, no ROM needed
$ node test-page.mjs /path/to/smw.sfc # full run, needs playwright
```

There is also a native binary (`cargo build --release`, then
`target/release/smw-restool-cli <rom> <out>`) running the identical code path
without a wasm runtime. It exists so the port can be diffed against the Python
directly; it is not a release artifact.

Roughly 11 ms per extraction including verification and instantiation, versus
~95 ms for the warm Python — but the real difference is 95 KB and no network
against ~10 MB of runtime plus PyPI installs.
