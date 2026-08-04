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
- [Architecture](architecture/index.md) — How it works
- [Parity Summary](parity-summary.md) — What's ported vs. not