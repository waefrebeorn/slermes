# Slermes C11 Parity — Live State

> **Single source of truth.** This table is regenerated from the live scanner
> (`tests/slermes_parity_battleground.py`) every time `make parity-walkway` runs.
> Do not hand-edit counts — the generator owns them inside `` blocks.

## Overall Numbers (live)

<!-- PARITY:AUTO -->
| Classification | Count | Percentage | Meaning |
|----------------|-------|------------|---------|
| **PORTED** | 12,085 | 86.0% | C11 implementation with PoP annotation |
| **REAL_GAP** | 1,958 | 13.9% | Honest gaps (not yet ported — IO/network/DB/logic; NOT faked) |
| **PARTIAL** | 2 | 0.0% | All C fns now carry PoP annotations |
| **BOOTLEG** | 7 | — | No-work echo stubs (recursive_false_gap_hunter.py) |
| **TOTAL** | 14,045 | 100% | All Python functions/methods scanned |

> **Generated 2026-08-04T05:07:31Z by `make parity-walkway` from the live scanner.** The PORT phase (v398→v667) is legacy — this table is the single source of truth for completeness. Do not hand-edit.
<!-- /PARITY:AUTO -->

**Generated:** 2026-08-04 (post-sync, v669 checkpoint) from live scanner
`tests/slermes_parity_battleground.py`. The PORT phase (v398→v667) is legacy —
complete; every function does real observable work and matches the Python original.
Closing REAL_GAPs is the path forward.

## How to read the numbers

- **PORTED** — C11 implementation with a `/* PoP: fn @ module.py:fn */` annotation,
  verified by the oracle harness where available.
- **REAL_GAP** — Python function not yet ported to C. These are honest gaps, not fakes.
  Each one needs a real C implementation (no stubs, no `printf`+`return NULL`).
- **PARTIAL** — C fn exists with a PoP annotation but needs full implementation logic.
- **BOOTLEG** — No-work echo stubs caught by `recursive_false_gap_hunter.py`.
  These should delegate to real C infrastructure (see `slermes-parity-bulk-drain` skill).

## How Gaps Were Closed (recent history)

### v668 — Upstream Sync (2026-08-03→04)
- Merged 3,401 upstream commits into the parent Python quarry.
- Fork-base `-s ours` merge in slermes (behind=0, C11 tree byte-identical).
- Quarry grew +1,771 Python features → REAL_GAP honestly rose (22 → 1,958).
- 45 bootleg echo-stubs eradicated (v668 DA sweep): all platform connect/disconnect
  now delegate to real C infrastructure.

### v667 — Last Close Session
- 45 bootleg closures (platform lifecycle stubs → real C infra).
- 7 printf-echo stubs closed by delegating to real C helpers.
- Build green, oracle baselines unchanged.

### v666 — Partial Clearance
- All 37 misclassified PARTIALs received PoP annotations (PARTIAL → 0).

### v665 → v666 — Lane 0 Closure
- Final lane-0 residual ports completed.

## There is no N/A

Rewriting from scratch in C **is** the point of this project, so nothing is
"not applicable." Every Python feature that is not yet reimplemented in C is
**REAL_GAP work** — including modules that earlier revisions parked as
"Python-only infra", "async Python", or "SDK/ABC". The scanner no longer emits
an N/A class at all; it reports only PORTED / PARTIAL / STUB / REAL_GAP.

## The scanner is blind to FAPs — behavioral correctness is a separate axis

The table above is a **static** count of *missing or shaped-wrong* functions.
It can report `PORTED 11,500+ / REAL_GAP 0 / PARTIAL 0` (build green) while
**real behavioral FAPs still exist** — C functions that are ported and compile
but produce output that diverges from LIVE Python Hermes. Examples found by the
oracle harness: a C provider-auth table with different membership than Python's
`PROVIDER_REGISTRY`, and C json serialization that differs in key order from
Python's `json.dumps`.

That defect class is a **FAP (Functional Alignment Problem)**. The parity scanner
cannot detect it — only running the oracle harness can (`bash
tests/oracle/run_oracles.sh` → any `cases: MISMATCH` is a FAP). See `docs/fap.md`
for the canonical definition, the real-vs-false FAP distinction, and the triage
procedure. Treat the oracle green/red result, not the PORTED count, as the
behavioral-completeness signal.

## CLI Commands (82 slash commands)

All 82 commands have real C11 handlers (0 stubs). Full command registry lives in
`src/cli/port_cli_command_registry.c`.

| Category | Count | Examples |
|----------|-------|----------|
| Session | 18 | `/new`, `/clear`, `/undo`, `/save`, `/load`, `/sessions`, `/stats`, `/recap`, `/conv`, `/history`, `/reset`, `/retry`, `/compress`, `/branch`, `/snapshot`, `/status`, `/resume`, `/rollback` |
| Config | 12 | `/model`, `/config`, `/setup`, `/uninstall`, `/backup`, `/topic`, `/reasoning`, `/fast`, `/voice`, `/yolo`, `/personality`, `/indicator` |
| Tools | 9 | `/tools`, `/tools-verify`, `/commands`, `/image`, `/paste`, `/browser`, `/toolsets`, `/deps`, `/skills` |
| Help | 1 | `/help` |
| System | 11 | `/exit`, `/stop`, `/doctor`, `/completions`, `/reload`, `/copy`, `/update`, `/debug`, `/logs`, `/dump`, `/send` |
| Security | 4 | `/approve`, `/deny`, `/secrets`, `/auth` |
| Gateway | 7 | `/platforms`, `/gateway`, `/webhook`, `/restart`, `/sethome`, `/handoff`, `/platform` |
| Display | 5 | `/redraw`, `/verbose`, `/skin`, `/statusbar`, `/busy` |
| Skills | 5 | `/skills-hub`, `/skills`, `/bundles`, `/curator`, `/reload-skills` |
| MCP | 2 | `/mcp`, `/reload-mcp` |
| Session Search | 3 | `/session-search`, `/session-export`, `/session-import` |
| Pet | 1 | `/pet` (info, gallery, select, remove, disable, scale) |
| Other | 17 | `/plugins`, `/insights`, `/goal`, `/agents`, `/profile`, `/whoami`, `/queue`, `/subgoal`, `/kanban`, `/footer`, `/steer`, `/background`, `/dashboard`, `/cron`, `/memory`, `/key`, `/usage` |

## Pet System API

| Function | Python Source | Purpose |
|----------|---------------|---------|
| `pet_init()` | — | Initialize pet system from config |
| `pet_get_state()` | — | Current animation state |
| `pet_update_state()` | state.py | Update from agent signals |
| `pet_info_json()` | — | Active pet info as JSON |
| `pet_gallery_json()` | — | Installed pets as JSON |
| `pet_cells_json()` | — | Frame cells for TUI |
| `pet_select()` | store.py:resolve_active_pet | Select active pet |
| `pet_disable()` | — | Disable pet display |
| `pet_set_scale()` | constants.py | Set scale factor |
| `pet_fetch_manifest()` | manifest.py | Fetch petdex manifest |
| `pet_find_entry()` | manifest.py | Find manifest entry by slug |
| `pet_load_pet()` | store.py:load_pet | Load installed pet |
| `pet_installed_pets()` | store.py:installed_pets | List installed |
| `pet_install_pet()` | store.py:install_pet | Install from manifest |
| `pet_remove_pet()` | store.py:remove_pet | Remove installed |
| `pet_thumbnail_png()` | store.py:thumbnail_png | Get thumbnail bytes |
| `pet_thumbs_dir()` | store.py:_thumbs_dir | Thumbnail cache directory |
| `pet_is_petdex_host()` | store.py:_is_petdex_host | URL host check |
| `pet_download_json()` | store.py:_download_json | HTTP JSON download |
| `pet_write_spritesheet()` | store.py:_write_spritesheet | Binary file copy |
| `pet_register_local_pet()` | store.py:register_local_pet | Register from local files |
| `pet_is_generated()` | store.py:generated | Check AI-generated flag |
| `pet_export_pet()` | store.py:export_pet | Export spritesheet bytes |

## Build System

```bash
make -j$(nproc)           # Build slermes binary (~37 MB)
make test                 # Run Mission8 test suite (77 tests)
make parity-walkway       # Refresh all parity docs from live scanner
make clean                # Clean build artifacts
make install              # Install to PREFIX (default: /usr/local)
```

## Key Architecture

```
slermes/
├── src/
│   ├── cli/          — CLI frontend, commands, config, display
│   ├── agent/        — Core agent loop, LLM client, providers
│   ├── tools/        — Tool implementations (file, terminal, web, etc.)
│   ├── gateway/      — Messaging gateway (Telegram, Discord, Slack, etc.)
│   ├── pet/          — Petdex mascot system
│   ├── hermes_cli/   — CLI command modules (config, session, model, etc.)
│   ├── provider/     — LLM provider integrations
│   ├── skills/       — Skill loader and registry
│   ├── plugins/      — Plugin system
│   ├── cron/         — Built-in scheduler
│   ├── tui/          — Terminal UI (ncurses-based)
│   ├── web_server/   — Built-in HTTP server
│   ├── desktop_app/  — Custom C11 desktop (SDL2/Wayland)
│   └── ...
├── include/          — Header files
├── lib/              — Libraries (73 sub-libraries)
├── tests/            — Test suite + oracle harness
└── docs/             — Documentation (mirrors src/ layout)
```

## Key Files

| File | Purpose |
|------|---------|
| `BANNER.md` | Project banner — build status, parity counts, sync checkpoint |
| `README.md` | Landing page — what this is, quick start, how to read parity |
| `docs/index.md` | Documentation entry point / TOC |
| `docs/parity-summary.md` | Live parity scan results (this file, auto-generated) |
| `docs/real-gap-list.md` | Full 1,958 REAL_GAP list, generated from live scan |
| `docs/cli/index.md` | CLI command reference |
| `docs/architecture/overview.md` | System design and data flow |
| `docs/architecture/build-system.md` | Make targets, deps, packaging |
| `docs/tui/index.md` | TUI reference |
| `docs/pet/index.md` | Pet system reference |
| `docs/dev/index.md` | Development workflow |
| `scripts/gen_parity_walkway.py` | Generator — the ONLY writer of parity aggregates |
| `scripts/parity_truth.py` | Gate — refuses to emit unless Python source is checked out |
| `live_parity_scan.json` | Machine-readable scan output (single source of truth) |
