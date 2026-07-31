# Slermes Documentation

**Slermes** is an in-progress C11 reimplementation of Hermes Agent — Nous
Research's AI agent framework. One binary, zero Python dependencies.

## Quick Links

- **[Getting Started](getting-started.md)** — Build, configure, run in 2 minutes
- **[Architecture](architecture/index.md)** — System overview, directory layout, build system
- **[CLI Commands](cli/index.md)** — Slash commands with examples
- **[TUI Reference](tui/index.md)** — Terminal UI features and RPC methods
- **[Pet System](pet/index.md)** — Petdex mascot installation and configuration
- **[Development](dev/index.md)** — Building, testing, contributing

## Project Status

This is a partial, actively-progressing port — roughly half of the upstream
Python surface is implemented in C so far, with the rest tracked as honest gaps.
The **[Parity Summary](parity-summary.md)** is the single source of truth: it is
regenerated from the live scanner (`tests/slermes_parity_battleground.py`) and
gives the exact PORTED / REAL_GAP / PARTIAL breakdown per module. Any completion
figure not in that file is stale — trust the scanner, not prose.
The scanner is static (it counts functions, it does not execute). Behavioral
correctness vs LIVE Python — **FAPs (Functional Alignment Problems)** — is tracked
separately by the oracle harness; see **[FAP](fap.md)**.

## Quick Start

```bash
git clone https://github.com/waefrebeorn/slermes
cd slermes
make -j$(nproc)
./slermes
```

### Dependencies

- gcc (C11)
- make
- libssl-dev
- libwayland-dev (Linux desktop)
- libxkbcommon-dev

See [Build System](architecture/build-system.md) for full details.
