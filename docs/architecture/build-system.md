<!-- PARITY:AUTO -->| PORTED  | 6,466 / 18,219 (35.5%) |
| REAL_GAP| 11,715 (64.3%) — no N/A |
|  ├ DA-4 stubs | 11 |
|  └ Genuine gaps | 11,704 |
| PARTIAL | 38 (0.2%) |
| BOOTLEG | 0 |
| **Watercup** | unavailable — interactive hermes CLI flows (approvals/kanban/curator/hello) block the oracle; sync counts below are from the live scanner and are current |\n**Phase (v673):** PORT phase

**Upstream sync:** 2,082 ahead / 1,056 behind (last merge 2026-10-06 (upstream fetched))

_Generated 2026-10-07T05:29:39Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Build System

Slermes uses a single `Makefile` with 24 build targets. No autotools, no cmake — pure GNU Make.

## Build Targets

| Target | Description |
|--------|-------------|
| `make` / `make all` | Build the `slermes` binary |
| `make clean` | Remove build artifacts |
| `make install` | Install to `PREFIX` (default: `/usr/local`) |
| `make uninstall` | Remove installed files |
| `make test` | Run test suite |
| `make docs` | Build documentation |
| `make packaging` | Create distribution packages |

## Quick Build

```bash
make -j$(nproc)
```

Output: `./slermes` (~47 MB static binary)

## Build Dependencies

| Package | Required | Notes |
|---------|----------|-------|
| gcc (C11) | Yes | `-std=c11` |
| GNU Make | Yes | libssl-dev | Yes | HTTPS support |
| libwayland-dev | Linux desktop | Wayland compositor |
| libxkbcommon-dev | Linux desktop | Keyboard handling |
| libegl1-mesa-dev | Linux desktop | GPU rendering |
| libgles2-mesa-dev | Linux desktop | GPU rendering |

## Installation

```bash
make install              # /usr/local (default)
make install PREFIX=~/opt # Custom prefix
```

Installs:
- `PREFIX/bin/slermes` — Main binary
- `PREFIX/share/slermes/` — Documentation and assets
- `PREFIX/share/man/man1/slermes.1` — Man page

## Build Details

The Makefile compiles:
- **352 C source files** in `src/`
- **73 libraries** in `lib/`
- **127 headers** in `include/`

Compilation flags:
- `-std=c11` — C11 standard
- `-O2 -g` — Optimized with debug symbols
- `-Wall` — Full warnings (many suppressed for third-party code)

## Packaging

```bash
make packaging
```

Creates:
- AppImage (Linux)
- Homebrew formula (macOS)
- NSIS installer (Windows)
- Docker image
- Nix flake

## Cross-Compilation

The CI targets 5 platforms:
- Linux x86_64 (native + test)
- macOS arm64
- Linux aarch64/armv7
- Windows x86_64 (cross-compile)
