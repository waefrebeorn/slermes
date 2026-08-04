# Slermes — Development Guide

Instructions for AI coding assistants and developers working on the **slermes**
C11 codebase. This is the C11 port of Hermes Agent, NOT the Python project.
The Python source lives in the parent repo (`/home/wubu/hermes-agent-dev/`)
and is the **oracle/spec** we port from — never edit it here.

**Never give up on the right solution.** Missing dependencies get built from
scratch in C with NO concessions; reuse wubu's own libs first.

## What Slermes Is

Slermes is a ground-up C11 reimplementation of Hermes Agent. One native binary
(`slermes`, ~37 MB), zero Python, zero C++. It mirrors the Python architecture:
CLI + TUI + messaging gateway (14+ platforms) + desktop app + web server.

The upstream Python repo (`NousResearch/hermes-agent`) is pulled into the
parent directory as the **quarry**. We port features from it to C. The C11
tree is maintained as a fork-base merge so upstream stays an ancestor
(`behind=0`); see `BANNER.md` for the canonical sync workflow.

## Hard Rules (sealed user edicts)

- **Pure C11 only.** `gcc -std=c11`. Zero C++. `nvcc` ONLY for PTX kernels.
- **No god headers, no monoliths.** Split files, reuse functions, opaque
  structs + minimal includes. Cohesive ports of ONE Python module are the
  correct boundary — do NOT split them speculatively.
- **Faithful, never stub.** Every port implements the real behavior of the
  Python original. No "not implemented" placeholders, no printf-echo stubs,
  no fake-looking code. The deliverable is a working binary backed by real
  tool output.
- **Name parity is the standard.** C CLI commands/aliases/functions must match
  Python 1:1. A different name is a bug to fix, not a C convention.
- **Reuse, don't duplicate.** Before creating a port file, grep `lib/` and
  `src/` for an existing backend/sibling. Small `port_*.c` files wrap existing
  static helpers — never duplicate lib logic.
- **No landlocked statics.** Oversized static arrays become hive-backed
  (`lib/libhive`, linked blocks + skipfield + freelist).
- **There is no N/A.** Every Python feature not yet in C is REAL_GAP work.

## How Porting Works

1. **The scanner is the source of truth.** `python3
   tests/slermes_parity_battleground.py --json` (or `make parity-walkway`)
   reads the Python tree + C source and classifies every Python function:
   PORTED (has `/* PoP: fn @ module.py:fn */` annotation + real C body),
   REAL_GAP (not ported), PARTIAL, BOOTLEG (echo stub).
2. **PoP annotations are single-line.** `/* PoP: fn @ mod.py:fn */` — one per
   Python function, exactly matching the scanner regex. Multi-line or
   `(ClassName)` forms are invisible to the scanner.
3. **Oracle-verify every port.** The harness (`tests/oracle/run_oracles.sh`)
   runs C functions against LIVE Python and reports `cases: MISMATCH` for
   behavioral divergence (FAPs). A green build is not enough.
4. **Build wiring:** ports register via `PHASE5_OBJ` (built fresh from
   subsystem `*_OBJ` lists). Every `.o` line MUST end with ` \` — a stray
   blank line triggers `missing separator`.

## Directory Layout

```
src/
├── cli/          — CLI frontend, commands (82 slash commands in
│                   port_cli_command_registry.c), config, display
├── agent/        — Core agent loop, LLM client, providers, context
├── tools/        — Tool implementations (terminal, file, web, browser)
├── gateway/      — Messaging gateway (Telegram, Discord, Slack, Signal,
│                   WhatsApp, Matrix, QQ, WeChat, Yuanbao, relay)
├── pet/          — Petdex mascot system
├── hermes_cli/   — CLI command modules (update_cmd, prompt_stash, ...)
├── provider/     — LLM provider integrations
├── skills/       — Skill loader and registry
├── plugins/      — Plugin system
├── cron/         — Built-in scheduler
├── tui/          — Terminal UI (ncurses)
├── web_server/   — Built-in HTTP server
├── desktop_app/  — Custom C11 desktop (SDL2/Wayland)
└── ...
include/          — Header files
lib/              — 73 internal libraries (libjson, libhttp, libtui,
                    libdb, libyaml, libproc, libhive, libregex, ...)
tests/            — Test suite + oracle harness + parity scanner
docs/             — Documentation (mirrors src/ layout)
scripts/          — version.txt, gen_parity_walkway.py, parity_truth.py
```

## Build & Test

```bash
make -j$(nproc)            # Build slermes binary (~37 MB)
make slermes               # Same (PHASE5_OBJ from subsystem *_OBJ lists)
make test                  # Mission8 unit tests
make parity-walkway        # Refresh ALL derived docs from live scanner
make fuzz                  # Fuzz tests
make desktop-gui           # Desktop app
```

**No header-dep tracking in the Makefile.** After touching a header, do
`rm -f slermes <touched>.o && make slermes` to force a real rebuild.

## Documentation Rules

- **Never hand-edit a count.** The scanner + `make parity-walkway` own every
  number. Hand-transcribed counts drift (the classic "8,688/8,688 100%"
  fiction) — that's why the purge exists.
- Sentinel blocks `<!-- PARITY:AUTO --> ... <!-- /PARITY:AUTO -->` are
  machine-owned; the generator overwrites them every run.
- `gen_parity_walkway.py` also purges stale claims (old counts, 99.8%, 100%,
  N/A) from all docs outside sentinels, so re-runs converge.
- Update docs via the generator or by writing prose that links to the
  generator-owned state — never by transcribing numbers.

## Current State

Live scanner (v669): **12,085 PORTED / 1,958 REAL_GAP / 2 PARTIAL / 7 BOOTLEG**
(14,045 total, 86.0%). Upstream sync: **0 behind** — repo is up to date.
See `docs/parity-summary.md` and `docs/real-gap-list.md`.

## Fork Sync

The canonical workflow (see `BANNER.md`):
1. Pull Python source into the parent repo (`hermes-agent-dev`).
2. In slermes: merge upstream with fork-base `-s ours` (C11 tree stays
   byte-identical, `behind=0`).
3. `make parity-walkway` to re-stamp all derived docs.
4. Commit + push. Never push the parent repo's C11 tree upstream.

## License

Waefrebeorn Umbrella License v3.0 — custom source-available license.
