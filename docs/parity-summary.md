# Slermes C11 Parity — Live State

**Generated:** 2026-07-27 (live scanner re-run) by `slermes_parity_battleground.py`

## Overall Numbers (live)

| Classification | Count | Percentage | Meaning |
|----------------|-------|------------|---------|
| **PORTED** | 6,975 | 59.4% | C11 implementation with PoP annotation |
| **REAL_GAP** | 4,769 | 40.6% | Honest gaps (not yet ported — IO/network/DB/logic; NOT faked) |
| **PARTIAL** | 0 | 0.0% | All C fns now carry PoP annotations (Lane 0 closed v666) |
| **STUB** | 0 | 0.0% | No stub functions remain |
| **TOTAL** | 11,744 | 100% | All Python functions/methods scanned |

> **Honesty note (2026-07-27):** live totals are 11,744 features (upstream
> grew from 9,733 at the 2026-07-23 re-pull to 11,744 — +2,011 new Python
> since the re-pull; +570 since the v621 snapshot). PORTED climbed 6,532 →
> 6,963 in the same window. REAL_GAP 4,619 → 4,781 because the quarry grew
> faster than ports — **expected and honest, not a regression.** PARTIAL is
> now **0** (all 37 misclassified PARTIALs received their PoP annotations in
> v666). v622 plan remains: close ~1,000 REAL_GAP via port_*/lib reuse
> (REUSE_GAP_PLAN_v622.md). First faithful reuse-port landed v667:
> `agent/billing_links.py` (5/5, oracle-verified).



> **This is a partial port, ~two-thirds done.** REAL_GAP is the honest count of Python
> features not yet reimplemented in C — it is not zero and the docs do not claim
> otherwise. Regenerate anytime with `python3 tests/slermes_parity_battleground.py
> --json`; this table is the single source of truth for completeness.

## History (older scans, for context)

> **✓ BOOTLEG ECHO-STUB ERADICATION COMPLETE (DA sweep, 2026-08-03):** the
> recursive bootleg hunter (`tests/recursive_false_gap_hunter.py`) dropped from
> **45 → 0 BOOTLEG** across 10,858 indexed functions. Two passes:
> (1) hunter false-positive fixes (fprintf(stderr) = real observability,
> array-field writes / ++-- / call-result assigns = real work, (void) casts
> neutralized, FILE_SCOPE bare-identifier returns, dotted Class.method +
> self.method() + await detection, correct PY_ROOT);
> (2) 45 true closures — every platform connect/disconnect now delegates to
> real C infra (`wx2`→weixin, `yb2`→yuanbao, `sgl`→signal, `qqa`→qqbot,
> `wac`→whatsapp, `wst`→ws_transport, pty close_all→registry drain), plus
> real logic ports (qqbot DM/group policy + op dispatch + media upload +
> exec-approval keyboard, honcho host-block migration, git resolution,
> env-poller loop, codex stdout drain, whatsapp two-step media download,
> weixin CDN+decrypt video cache, claude setup-token spawn, vision encode,
> registry standalone send, voice shutdown, compression heartbeat thread,
> LSP shutdown, MCP refresh, env/modal cleanup, stream-consumer run,
> debounce delay, billing PATCH, daytona state, online-research session,
> transcription spec probe). `make slermes` 0 errors; new accessors confirmed
> `T` in `nm slermes`; oracle baselines unchanged (pre-existing MISMATCHes).

Older overall snapshot (end v551): PORTED 4,881 (50.2%), REAL_GAP 4,802 (49.3%),
PARTIAL 48, STUB 0 (N/A category since removed — there is no N/A).

> **✓ RESIDUAL FACADE + NO-RETURN + THIN-FRAUD ERADICATION COMPLETE (v548):** on
> top of v547's 110, v548 eradicated **95 more** fraudulent/dormant ports found by
> a fresh mechanical scan (the v546 audit's "42 thin + 37 no-return" hand counts
> were stale — the real mechanical counts were 54 thin + 25 no-return + 56 façade).
> Breakdown: **53 residual façades** (`return <const>`/`void` no-op), **25
> no-return `(void)arg` bodies**, **17 thin-wrapper frauds** (Python did real work,
> C returned a canned/const). All deleted via the v547 edict-#2 method (read REAL
> Python → honest verdict → delete fake `PoP:` + body → honest REAL_GAP).
> Verified: **0 `PoP:` comments remain** for any eradicated name; **0 stubs, 0 N/A**.
> 26 honest-limitation façades retained (v547's 23 SDK-getters/`__enter__`/`__exit__`
> + v548's 3 SDK-getters + stateless scrubber reset). Result: PORTED 4,931 →
> **4,881** (−50); REAL_GAP 4,754 → **4,802** (+48); PARTIAL 46 → **48** (+2).
> Build: clean (0 errors), `run_mission8_tests.sh`: **36 passed / 0 failed / 35
> skipped**. The v543–v546 oracle-verified leaf closures are untouched.

> **Honesty note (v545):** v545 resumed auto-pilot and ran the real re-scan
> the v544 prompt demanded. The "exhausted pure supply" assumption was WRONG —
> 69 single-gap pure-leaf candidates remained. v545 closed 6 genuine gaps with
> faithful, oracle-verified ports (no dupes, no façades):
> `gateway/display_config.py:_normalise` (fixed a pre-existing drifted façade
> `normalise_display_value` that only lowercased — now faithful per-setting,
> 16/16 oracle), `gateway/scale_to_zero.py:_platform_name` (6/6),
> `gateway/whatsapp_identity.py:to_whatsapp_jid` (11/11),
> `hermes_cli/pty_bridge.py:_clamp_dimension` + `win_pty_bridge.py:_clamp`
> (11/11), and `tools/fuzzy_match.py:_map_normalized_positions` (3/3).
> End-v545 REAL_GAP: **4,703** (down 6 from v544's 4,709), PORTED 4,983
> (up 6). Every port verified byte-equivalent to LIVE Python via harness +
> oracle (47 cases total, 0 mismatches).
>
> **Honesty note (v544):** the v544 work extended existing `*_helpers.c`
> files with genuine, oracle-verified leaf ports — no new parallel files, no
> false cross-credits. Closures: `port_learning_graph_render_helpers.c`
> (+5 leaves: `_to_ts`, `_period_key`, `_period_label`, `_node_score`,
> `_node_meta`, on top of v543's 6), `port_gateway_response_filters.c`
> (`is_partial_silence_marker` + a marker-set fix to match LIVE Python's
> `LIVE_GATEWAY_SILENT_MARKERS` exactly), and `port_gateway_signal_format.c`
> (new `markdown_to_signal` faithful port, PCRE2-backed). Also found and fixed
> a pre-existing faithfulness bug in `response_filters` (its marker set had 7
> entries vs Python's 4). End-v544 REAL_GAP: **4,709** (down 13 from v543's
> 4,722), PORTED 4,977 (up 13). Every port verified byte-equivalent to LIVE
> Python via harness + oracle (learning_graph_render 35/35, response_filters
> 38/38, signal_format 16/16).

> **Honesty note (v546):** v546 re-ran the live re-scan and continued pure-leaf
> closure, extending EXISTING port files (no parallel dupes) plus one legit new
> file (`port_agent_oneshot.c` — `agent/oneshot.py` had no prior port). All 11
> ports oracle-verified byte-equivalent to LIVE Python (75 cases, 0 mismatches):
> `agent/error_classifier.py` (`_is_openrouter_upstream_error`,
> `_extract_upstream_provider_name`, 13/13), `tools/tool_result_storage.py`
> (`_safe_result_filename` — re + SHA256, 8/8), `agent/model_metadata.py`
> (`is_output_cap_error`, 12/12), `agent/display.py` (shell-summarization
> cluster `_split_shell_words`/`_strip_shell_pipe_tail`/`_split_shell_compound`/
> `_clean_shell_segment`/`_is_shell_boundary_echo`, 19/19),
> `tools/xai_http.py` (`_coerce_expires_after`, 14/14), `agent/oneshot.py`
> (`_strip_code_fence`, 9/9). Two bugs were caught against LIVE Python BEFORE
> sign-off: a SHA256 digest length (24→12 hex chars) and a `splitlines()`
> trailing-newline mismatch. End-v546 REAL_GAP: **4,692** (down 11 from v545's
> 4,703), PORTED 4,994 (up 11). The "pure supply exhausted" claim was again
> proven FALSE — 7 modules with existing port files still held pure leaves.

## There is no N/A

Rewriting from scratch in C **is** the point of this project, so nothing is
"not applicable." Every Python feature that is not yet reimplemented in C is
**REAL_GAP work** — including modules that earlier revisions parked as
"Python-only infra", "async Python", or "SDK/ABC". The scanner no longer emits
an N/A class at all; it reports only PORTED / PARTIAL / STUB / REAL_GAP. Any
older note below that called a subsystem "N/A" was reclassified as REAL_GAP.

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

## How Gaps Were Closed (historical)

### Batch 3 — Missing PoP Annotations (8 gaps closed)
- `json_obj_get` → auxiliary_client `_obj_get`
- `build_payload` → journey `_build_payload`
- `skill_bundles_print` → pets `_print`
- `voice_set_enabled` → pets `_set_enabled`

## CLI Commands

**95 slash commands**, all with real C11 handlers (0 stubs):

| Category | Count | Examples |
|----------|-------|---------|
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
make -j$(nproc)           # Build slermes binary
make install              # Install to PREFIX (default: /usr/local)
make clean                # Clean build artifacts
make test                 # Run test suite
make docs                 # Build documentation
make packaging            # Create distribution packages
```

## Verification

Run full parity scan:
```bash
python3 tests/slermes_parity_battleground.py --json
```

Check specific module:
```bash
python3 tests/slermes_parity_battleground.py --detail --module agent/pet/store.py
```

## Key Architecture

```
slermes/
├── src/
│   ├── cli/          — CLI frontend, commands, config, display
│   ├── agent/        — Core agent loop, LLM client, providers
│   ├── tools/        — Tool implementations (file, terminal, web, etc.)
│   ├── gateway/      — Messaging gateway (Telegram, Discord, etc.)
│   ├── pet/          — Petdex mascot system
│   ├── acp/          — Agent Communication Protocol
│   ├── cron/         — Scheduled task runner
│   ├── provider/     — OAuth providers
│   ├── skills/       — Skills parser
│   └── plugins/      — Plugin system
├── include/          — Header files (127 total)
├── lib/              — Libraries (73 sub-libraries)
├── tests/            — Test suite
└── docs/             — Documentation
```
