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

# Getting Started

Build, configure, and run Slermes.

## Build

**Linux**

```bash
git clone https://github.com/waefrebeorn/slermes
cd slermes
make -j$(nproc) slermes
```

**Windows (MSYS2 + winlibs — the primary development platform)**

```bash
git clone https://github.com/waefrebeorn/slermes
cd slermes
# inside MSYS2 bash:
source build/winlibs-env.sh
make -j8 slermes USERPROFILE='C:\Users\<you>' LOCALAPPDATA='C:\Users\<you>\AppData\Local'
```

This produces a single native binary: `./slermes` on Linux, `slermes.exe`
on Windows (the current Windows release build is ~12 MB with whisper
support; size varies with profile and linked libs — `PROFILE=dev` links a
much larger unstripped debug binary).

Toolchain rules that matter on the dev box (full doctrine:
[dev/gates.md](dev/gates.md)):

- `build/winlibs-env.sh` must be sourced **in the same shell** that runs
  make — it puts winlibs GCC first and sets the prebuilt lib paths.
- `USERPROFILE`/`LOCALAPPDATA` go as **make variables**, not exports: this
  box's shell drops exported env vars before native children see them, and
  ccache refuses to run without them.
- Use the explicit target `make slermes` — bare `make` historically built
  only the first object and reported success without compiling anything.
- No Python runtime is needed to build or run; Python is only for the
  verification tooling (oracles, scanners, generators).

## Run

```bash
./slermes          # Linux
./slermes.exe      # Windows
```

The entry point (`src/main.c`) resolves its home as `$SLERMES_HOME` →
`~/.slermes` → `~/.hermes`, loads `.env` from there (credential values are
ASCII-sanitized), and dispatches on argv:

| Invocation | What it does |
|---|---|
| `slermes` | interactive CLI (line editor, slash commands) |
| `slermes "message"` | one-shot conversation |
| `slermes --tui` | full-screen TUI |
| `slermes gateway` | multi-platform gateway |
| `slermes cron` | scheduler |
| `slermes setup` / `init` | configuration wizard / init |
| `slermes doctor` | config + key health check |
| `slermes acp` | ACP server (JSON-RPC over stdio) |
| `slermes mcp-serve [port]` | MCP HTTP server (default 9100) |
| `slermes api-server [port]` | OpenAI-compatible REST API (default 9101) |
| `slermes update` | self-update |
| `slermes completions bash\|zsh` | shell completions |
| `slermes --version` / `--help` | banner / usage |

Top-level flags: `--json`, `--session <id>`, `--profile <name>`,
`--eval/-e`, `--tui`. Unknown leading-dash flags exit 2 (argparse parity).

> Dev-box note: on the Windows dev machine Smart App Control can refuse
> freshly linked binaries (rc 127 with no output); this is an environment
> block, not a build failure — the compile/link gates remain valid evidence.
> See [POST_REBOOT_RESUME](../POST_REBOOT_RESUME.md).

## Basic configuration

Config lives under the resolved home (`~/.slermes` by default):
`config.yaml` for settings, `.env` for secrets only.

```bash
slermes setup            # interactive wizard
slermes doctor           # verify config + keys
```

Or edit `~/.slermes/.env` directly:

```
OPENAI_API_KEY=sk-...
ANTHROPIC_API_KEY=sk-ant-...
```

In-session slash commands (`/model`, `/key`, `/setup`, …) are listed in
[cli/index.md](cli/index.md).

## Next steps

- [CLI Commands](cli/index.md) — command reference
- [Architecture](architecture/overview.md) — how it works
- [Work Dashboard](WORK_DASHBOARD.md) — where the port stands and what's next
- [Parity Summary](parity-summary.md) — what's ported vs. not
