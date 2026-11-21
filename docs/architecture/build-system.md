<!-- PARITY:AUTO -->| PORTED  | 21,129 / 49,699 (42.5%) |
| REAL_GAP| 28,532 (57.4%) — no N/A |
|  ├ DA-4 stubs | 26 |
|  └ Genuine gaps | 28,506 |
| PARTIAL | 38 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | unavailable — partial sweep only: 24 of ~1685 port gates completed before the pipeline's 120s stage timeout; a full watercup sweep is required to refresh this number (results.jsonl is untracked build output) |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-21T21:16:55Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Build System

Pure GNU Make — no autotools, no cmake. The top-level `Makefile` includes
`build/config.mk` (flags + profiles), `build/objects.mk` (cupboard-organized
object lists), `build/rules.mk` (pattern rules), `build/targets.mk` (binary
targets) and `build/test.mk` (per-port test targets). Run `make help` for the
live target list.

## Primary targets

| Target | Description |
|--------|-------------|
| `make slermes` | Build the `slermes` binary (the explicit, honest target) |
| `make all` / `make` | Default goal → the binary |
| `make clean` | Remove build artifacts (also clears the `.profile` stamp) |
| `make test` | Build + run the test suite |
| `make install` / `uninstall` | Install to `PREFIX` (default `/usr/local`) |
| `make plugins` | Build the loadable `.so` plugins |
| `make desktop-gui` | SDL2 desktop GUI binary |
| `make parity` | scan → watercup → walkway (refresh every parity sentinel) |
| `make org-parity` | structural module ↔ C-home scan |
| `make gap-triage` | ranked gap list |
| `make dist-appimage` / `dist-docker` / `dist-nix` / `dist-nsis` | packaging |
| `make static` | `-static -Os -s` static binary |
| `make asan` / `valgrind` / `coverage` / `fuzz` / `static-analysis` | diagnostics |

**Use the explicit target `make slermes`.** Bare `make` historically built
only the first object in an included `.mk` and exited 0 without compiling
anything; `.DEFAULT_GOAL` is now pinned, but the explicit target is the
honest habit. Never verify a change with `make | grep -i error`.

## Build profiles (`make PROFILE=<name>`)

| Profile | Flags | Use |
|---|---|---|
| `release` (default) | `-O3` + LTO | the shipped binary |
| `dev` | `-O0 -g3` | fastest rebuild, easiest to diff against the oracle |
| `strict` | `-O2 -g` + full warning set | the bar before committing new work |
| `analyzer` | `-O1 -g -fanalyzer` | static NULL-deref / OOB / leak analysis |
| `sanitize` | `-O1 -g -fsanitize=address,undefined` | needs a toolchain with the runtimes; **refuses loudly** when absent — a sanitizer that did not run is not a pass |

An unknown profile is a hard error. `PROFILE` is not an object-file
dependency, so switching it on a built tree requires `make clean` (the
`build/.profile` stamp guards the mismatch). `dev` links a large unstripped
binary; `release` is the small shipped one — say which profile produced a
binary before quoting its size.

## Quick build

```bash
# Linux / macOS
make -j$(nproc) slermes

# Windows (one MSYS2 bash child; this box drops exported env vars)
/c/msys64/usr/bin/bash -lc 'source /c/Users/eman5/slermes/build/winlibs-env.sh; \
  cd /c/Users/eman5/slermes && make -j8 slermes \
  USERPROFILE="C:\Users\eman5" LOCALAPPDATA="C:\Users\eman5\AppData\Local"'
```

Output: `./slermes` / `slermes.exe`. The current Windows release build is
~12 MB with whisper support (size varies with profile and linked libs).

## Toolchain & dependencies

- **C11** (`-std=c11`), no GNU extensions in project code, no C++, no VLAs.
- **Windows**: MSYS2 (bash/make/python) + winlibs GCC under `C:/winlibs`.
  `build/winlibs-env.sh` puts winlibs first, sets lib paths, unsets
  `GCC_EXEC_PREFIX`/`COMPILER_PATH`. `USERPROFILE`/`LOCALAPPDATA` go as
  **make variables** (ccache refuses without them). ccache is wired into `CC`.
- **Linux**: `build-essential libssl-dev libwayland-dev libxkbcommon-dev
  libegl1-mesa-dev libgles2-mesa-dev`.
- **macOS**: Xcode command-line tools + `brew install openssl`.
- Link line (Windows): `-lws2_32 -lgdi32 -lz -lpcre2-8 -ltre -lssl -lcrypto`
  (+ SDL2 for the GUI), `-L C:/winlibs/extra-libs` ahead of them. No
  `libtinfo` (folded into ncursesw); no `-lasound`/`-lwebp` on Windows.

The three-flag rule: `-include hermes_win32.h -D_GNU_SOURCE -I include/compat`
belong in every compile path (CFLAGS **and** the pattern rules) — a merge
that trims them makes every shim invisible.

## Object organization

`build/objects.mk` is cupboard-organized: one `CUPBOARD_*` per subsystem;
`CLI_OBJ`/`PORT_OBJ` concatenate them. After editing run
`python scripts/organize_objects.py` (refuses to write unless the expanded
object multiset is identical). **Registered ≠ reachable** — confirm a new
object is on the link line: `make -n slermes | grep -c '<obj>.o'`.

## Packaging

`packaging/{appimage,docker,homebrew,nix,nsis}` + `make dist-*` targets.
Release engineering is downstream of parity (see [ROADMAP](../../ROADMAP.md)).
