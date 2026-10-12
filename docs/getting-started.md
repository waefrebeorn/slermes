<!-- PARITY:AUTO -->| PORTED  | 6,466 / 18,219 (35.5%) |
| REAL_GAP| 11,715 (64.3%) — no N/A |
|  ├ DA-4 stubs | 11 |
|  └ Genuine gaps | 11,704 |
| PARTIAL | 38 (0.2%) |
| BOOTLEG | 0 |
| **Watercup** | unavailable — interactive hermes CLI flows (approvals/kanban/curator/hello) block the oracle; sync counts below are from the live scanner and are current |\n**Phase (v673):** PORT phase

**Upstream sync:** 2,082 ahead / 1,056 behind (last merge 2026-10-06 (upstream fetched))

_Generated 2026-10-07T05:29:39Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Getting Started

Build, configure, and run Slermes in 2 minutes.

## Build

```bash
git clone https://github.com/waefrebeorn/slermes
cd slermes
make -j$(nproc)
```

This produces a single binary: `./slermes` (~37 MB).

## Run

```bash
./slermes
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