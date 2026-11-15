<!-- PARITY:AUTO -->| PORTED  | 8,778 / 34,262 (25.6%) |
| REAL_GAP| 25,451 (74.3%) — no N/A |
|  ├ DA-4 stubs | 43 |
|  └ Genuine gaps | 25,408 |
| PARTIAL | 33 (0.1%) |
| BOOTLEG | 0 |
| **Watercup** | not run — no results under tests/watercup/build/; the scanner counts above are current |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-15T07:30:31Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Getting Started

Build, configure, and run Slermes in 2 minutes.

## Build

**Linux**

```bash
git clone https://github.com/waefrebeorn/slermes
cd slermes
make -j$(nproc)
```

**Windows (MSYS2 — the primary development platform)**

```bash
git clone https://github.com/waefrebeorn/slermes
cd slermes
source build/winlibs-env.sh
make -j8 slermes USERPROFILE='C:\Users\<you>' LOCALAPPDATA='C:\Users\<you>\AppData\Local'
```

This produces a single binary: `./slermes` (~44 MB with whisper). On Windows it
is `slermes.exe`. `build/winlibs-env.sh` must be sourced first — it sets the
compiler and the prebuilt third-party lib paths, and this bash drops exported
env vars.

## Run

```bash
./slermes          # Linux
./slermes.exe      # Windows
```

On first run, Slermes creates `~/.slermes/` and launches the setup wizard. You'll need to configure at least one LLM provider.

## Basic Configuration

### API Key

```bash
/key set openai
```

Or set `HERMES_OPENAI_API_KEY` in `~/.slermes/.env`.

### Model

```bash
/model list
/model set gpt-4
```

### Setup Wizard

```bash
/setup
```

Or non-interactive:

```bash
/setup --quick
```

## Next Steps

- [CLI Commands](cli/index.md) — Full command reference
- [Pet System](pet/index.md) — Install a mascot
- [Architecture](architecture/overview.md) — How it works
- [Parity Summary](parity-summary.md) — What's ported vs. not