# Slermes Documentation

**Slermes** is a complete C11 reimplementation of Hermes Agent — Nous Research's AI agent framework. One binary, zero Python dependencies.

## Quick Links

- **[Getting Started](getting-started.md)** — Build, configure, run in 2 minutes
- **[Architecture](architecture/index.md)** — System overview, directory layout, build system
- **[CLI Commands](cli/index.md)** — All 95 slash commands with examples
- **[TUI Reference](tui/index.md)** — Terminal UI features and RPC methods
- **[Pet System](pet/index.md)** — Petdex mascot installation and configuration
- **[Development](dev/index.md)** — Building, testing, contributing

## Project Status (v510)

| Metric | Value |
|--------|-------|
| **PORTED** | 8,701 / 9,731 (89.4%) |
| **REAL_GAP** | 0 |
| **CLI Commands** | 95 (all real C11 handlers) |
| **TUI RPC Methods** | 92 registered |
| **Binary Size** | ~47 MB |
| **Source Files** | 352 C files + 127 headers |
| **Libraries** | 73 sub-libraries |

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
