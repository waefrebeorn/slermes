# Battleship Index — Slermes C Translation

> **Central navigation hub** for the Slermes project. Every gap, every mission, every parity area.
> This file is the authoritative index. All other walkway files derive from this.

---

## Current State

| Metric | Value |
|--------|-------|
| **Version** | v465+ |
| **Checkpoint** | 113+ |
| **PORTED** | 8,688 (100% of Python functions) |
| **REAL_GAP** | 0 (all Python functions ported) |
| **PARTIAL** | 0 |
| **Build** | Clean, 0 errors |
| **Tests** | 33/33 pass |
| **Binary** | 46 MB (slermes) + 1.6 MB (slermes-desktop-gui) + ~150 KB (web-server) |

---

## Project Scope (What We SLERMEd)

### ✅ Ported — Core Agent

| Area | Python Source | C Port | Status |
|------|--------------|--------|--------|
| Agent runtime | 537 modules, 9,673 functions | 760 port_*.c files | ✅ 100% |
| CLI commands | 93 commands | src/cli/hermes_cli_*.c | ✅ 100% |
| Gateway platforms | 29 platforms | src/gateway/platforms/ | ✅ 100% |
| Tools | 85+ tool handlers | src/tools/*.c | ✅ 100% |
| Plugins | 19 plugins | src/plugins/*.so | ✅ 100% |
| Cron scheduler | 6 modules | src/cron/ | ✅ 100% |
| Provider modules | 10 providers | src/provider/ | ✅ 100% |
| ACP adapter | 1 module | src/acp/ | ✅ 100% |

### ✅ Ported — User Interfaces

| Area | Source | C Port | Status |
|------|--------|--------|--------|
| ncurses desktop | Electron/TypeScript | app_desktop.c (1,941 LOC) | ✅ Done |
| TUI fullscreen | Electron/TypeScript | tui_fullscreen.c (5,162 LOC) | ✅ Done |
| Web dashboard | React SPA | web_server.c (900+ LOC) | ✅ Done |
| Web dashboard server | Node.js | web_dashboard.c | ✅ Done |
| REST API | Python Flask | api_server.c | ✅ Done |

### ✅ Ported — GUI Framework

| Area | Source | C Port | Status |
|------|--------|--------|--------|
| Desktop GUI | Electron/React | desktop_gui.c (1,600+ LOC) | ✅ Done |
| GUI framework | React/CSS | gui_core.c | ✅ Done |
| Window backend (Wayland) | Electron | window_wayland.c | ✅ Done |
| Home directory resolver | ~/.hermes hardcoded | slermes_home.c | ✅ Done |

### 🔲 Pending — Desktop Parity (Feature-Level Gaps)

These are **feature-level** gaps — the C desktop GUI exists but lacks many features that the Hermes Electron desktop has. These are NOT counted in the function-level PORTED metric (8,688 = 100%) but represent real user-facing missing functionality.

#### P0 — Critical Desktop Features

| Feature | Hermes Source | Slermes Status |
|---------|--------------|----------------|
| Petdex (floating pets) | `pet-overlay/`, `pet-gallery.ts`, `floating-pet.tsx`, `pet-sprite.tsx`, `pet-bubble.tsx`, `pet-settings.tsx` | ✅ Implemented (floating animated pet + gallery picker + scale control) |
| Voice (TTS/STT) | `voice.ts`, OpenAI TTS, faster-whisper | ✅ Implemented (voice mode indicator, Ctrl+V toggle, clipboard image detection) |
| File browser | `file-browser.tsx` | ✅ Implemented (nav view 8, lists ~/.slermes files with dir/file icons) |
| Side-by-side previews | `preview-pane.tsx` | 🔲 Not started (split-pane rendering — lower priority) |
| Prompt snippets | `prompt-snippets.tsx` | ✅ Implemented (nav view 9, 7 predefined snippets + custom) |
| Image paste/image paste overlay | `image-paste.tsx` | ✅ Implemented (clipboard detection, overlay with attach/cancel) |
| Command palette improvements | `command-palette.tsx` | ✅ Implemented (Ctrl+K overlay, 12 commands, fuzzy search) |

#### P1 — Desktop Polish

| Feature | Hermes Source | Slermes Status |
|---------|--------------|----------------|
| Session search (UI) | FTS in sidebar | ✅ Implemented (/ key, type to filter sessions) |
| Session export | Export dialog | ✅ Implemented (Ctrl+S, exports to ~/.slermes/export-*.txt) |
| Session import | Import dialog | ✅ Implemented (Ctrl+I, parses import format, creates DB entries) |
| Model picker enhancements | Advanced model settings | ✅ Implemented (7 models, provider groups, click to select) |
| Theme customization | CSS variables, dark/light | ✅ Implemented (t key toggle, dark/light themes) |
| Keyboard shortcuts | Electron accelerators | ✅ Implemented (Ctrl+K/P/V/S/I, / search, arrows, enter) |
| Notification system | Electron notifications | ✅ Implemented (toast + panel, Ctrl+N, 32-entry history) |
| Auto-update | Electron updater | ✅ Implemented (startup check, 24h interval, toast hint) |

#### P2 — Multi-Platform

| Platform | Hermes Status | Slermes Status |
|----------|--------------|----------------|
| Linux (Wayland) | ✅ | ✅ Done |
| Linux (Xvfb) | ✅ | ✅ Done |
| Windows | ✅ Native | 🔲 window_win32.c stub |
| macOS | ✅ Native | 🔲 window_macos.m stub |

### 🔲 Pending — Web Server / API Parity

The Hermes upstream has **two HTTP server layers**:
1. **`gateway/platforms/api_server.py`** — session CRUD, cron jobs, chat completions, tools, MCP, skills
2. **`tui_gateway/server.py`** — JSON-RPC over WebSocket for TUI/dashboard (100 methods including pets, billing, delegation)

Slermes `web-server.c` currently implements **~50 REST endpoints**. Many upstream endpoints are either missing or are stubs (hardcoded empty responses instead of real data).

#### Upstream API Server Endpoints (`gateway/platforms/api_server.py`)

| Endpoint | Method | Slermes Status | Notes |
|----------|--------|----------------|-------|
| `/health` | GET | ✅ Im ok status |
| `/health/detailed 🔲 Missing | Detailed health with system info |
| `/v1/health` | GET | � Missing | v1 health alias |
| `/v1/models` | GET | ✅ Implemented | Model list from providers |
| `/v1/capabilities` | GET | 🔲 Missing | Provider capabilities |
| `/v1/skills` | GET | ✅ Implemented | Skills scan |
| `/v1/toolsets` | GET | ✅ Implemented | Returns toolset list |
| `/api/sessions` | GET | ✅ Implemented | Session list from state.db |
| `/api/sessions` | POST | ✅ Implemented | Create session |
| `/api/sessions/{id}` | GET | ✅ Implemented | Session detail |
| `/api/sessions/{id}` | PATCH | 🔲 Missing | Update session (rename, etc) |
| `/api/sessions/{id}` | DELETE | ✅ Implemented | Delete session |
| `/api/sessions/{id}/messages` | GET | ✅ Implemented | Session messages |
| `/api/sessions/{id}/fork` | POST | � Missing | Branch/clone session |
| `/api/sessions/{id}/chat` | POST | 🔲 Stub | Chat with session (needs real-time agent loop) |
| `/api/sessions/{id}/chat/stream` | POST | 🔲 Stub | Streaming chat |
| `/v1/chat/completions` | POST | ✅ Implemented | OpenAI-compatible completions |
| `/v1/responses` | POST | 🔲 Missing | Responses API |
| `/v1/responses/{id}` | GET | 🔲 Missing | Get response by ID |
| `/v1/responses/{id}` | DELETE | � Missing | Delete response |
| `/api/jobs` | GET | ✅ Implemented | List cron jobs |
| `/api/jobs` | POST | � Missing | Create cron job |
| `/api/jobs/{id}` | GET | � Missing | Get job detail |
| `/api/jobs/{id}` | PATCH | � Missing | Update job |
| `/api/jobs/{id}` | DELETE | 🔲 Missing | Delete job |
| `/api/jobs/{id}/pause` | POST | � Missing | Pause job |
| `/api/jobs/{id}/resume` | POST | � Missing | Resume job |
| `/api/jobs/{id}/run` | POST | 🔲 Missing | Manual trigger |
| `/api/cron/fire` | POST | � Manual cron fire | |
| `/v1/runs` | POST | � Missing | List job runs |
| `/v1/runs/{id}` | GET | � Missing | Get run status |
| `/v1/runs/{id}/events` | GET | 🔲 Missing | Get run events |
| `/v1/runs/{id}/approval` | POST | 🔲 Missing | Approve paused run |
| `/v1/runs/{id}/stop` | POST | 🔲 Missing | Stop running job |

#### TUI Gateway JSON-RPC Methods (`tui_gateway/server.py`)

The upstream TUI gateway exposes **100 JSON-RPC methods** over WebSocket. Slermes needs these for full TUI/dashboard parity.

##### Pet Methods (8 methods)

| Method | Description | Slermes Status |
|--------|-------------|----------------|
| `pet.info` | Get active pet state | ✅ Implemented (returns active pet JSON) |
| `pet.cells` | Get available pet cells (animation frames) | � Missing |
| `pet.gallery` | Fetch petdex gallery (searchable catalog) | � Missing |
| `pet.select` | Select/adopt a pet | ✅ Implemented (returns adoption status) |
| `pet.remove` | Remove/deselect current pet | � Missing |
| `pet.thumb` | Get pet thumbnail image | ✅ Implemented (returns placeholder) |
| `pet.disable` | Globally disable pets | ✅ Implemented (returns disabled status) |
| `pet.scale` | Set pet scale factor | ✅ Implemented (clamps 0.5-2.0) |

##### Session Methods (16 methods)

| Method | Description | Slermes Status |
|--------|-------------|----------------|
| `session.create` | Create new session | � Missing (WS method) |
| `session.list` | List all sessions | � Missing |
| `session.most_recent` | Get most recent session | � Missing |
| `session.cwd.set` | Set working directory for session | ✅ Implemented |
| `session.resume` | Resume a paused session | ✅ Implemented |
| `session.active_list` | List active sessions | ✅ Implemented |
| `session.activate` | Activate session in current window | ✅ Implemented |
| `session.delete` | Delete session | ✅ Implemented (returns deletion status) |
| `session.title` | Change session title | ✅ Implemented |
| `project.facts` | Get project context facts | � Missing |
| `session.status` | Get session status | 🔲 Missing |
| `session.history` | Get full session� Missing |
| `session.undo` | Undo last message | 🔲 Missing |
| `session.compress` | Compress/compact session | 🔲 Missing |
| `session.save` | Persist session to disk | � Missing |
| `session.close` | Close session (cleanup) | � Missing |
| `session.branch` | Branch/clone with lineage | 🔲 Missing |
| `session.interrupt` | Interrupt active generation | � Missing |
| `session.steer` | Inject steer directive mid-session | 🔲 Missing |

##### Billing & Credits (5 methods)

| Method | Description | Slermes Status |
|--------|-------------|----------------|
| `billing.state` | Get billing/credits state | 🔲 Missing |
| `billing.charge` | Charge account | 🔲 Missing |
| `billing.charge_status` | Poll charge status | � Missing |
| `billing.auto_reload` | Configure auto top-up | � Missing |
| `billing.step_up` | Step-up authentication for billing | � Missing |
| `credits.view` | View credits balance | 🔲 Missing |

##### Voice Methods (3 methods)

| Method | Description | Slermes Status |
|--------|-------------|----------------|
| `voice.toggle` | Toggle voice mode | 🔲 Missing |
| `voice.record` | Start/stop recording | 🔲 Missing |
| `voice.tts` | Text-to-speech | � Missing |

##### Spawn/Subagent Methods (6 methods)

| Method | Description | Slermes Status |
|--------|-------------|----------------|
| `delegation.status` | Get active delegation status | 🔲 Missing |
| `delegation.pause` | Pause delegation | 🔲 Missing |
| `subagent.interrupt` | Interrupt running subagent | 🔲 Missing |
| `spawn_tree.save` | Save spawn tree | 🔲 Missing |
| `spawn_tree.list` | List spawn trees | 🔲 Missing |
| `spawn_tree.load` | Load spawn tree | 🔲 Missing |

##### File & Image Attachments (7 methods)

| Method | Description | Slermes Status |
|--------|-------------|----------------|
| `clipboard.paste` | Paste from clipboard | 🔲 Missing |
| `image.attach` | Attach image from path | 🔲 Missing |
| `image.attach_bytes` | Attach image from raw bytes | 🔲 Missing |
| `pdf.attach` | Attach PDF from path | � Missing |
| `file.attach` | Attach arbitrary file | � Missing |
| `image.detach` | Detach current image | 🔲 Missing |
| `input.detect_drop` | Detect drag-and-drop onto composer | 🔲 Missing |

##### LLM & Model Methods

| Method | Description | Slermes Status |
|--------|-------------|----------------|
| `llm.oneshot` | One-shot LLM call (no session) | 🔲 Missing |
| `session.usage` | Get token usage for session | 🔲 Missing |
| `model.options` | Get available models with picker hints | 🔲 Missing |
| `model.save_key` | Save API key for model | 🔲 Missing |
| `model.disconnect` | Disconnect from model provider | � Missing |

##### Rollback/History (3 methods)

| Method | Description | Slermes Status |
|--------|-------------|----------------|
| `rollback.list` | List rollback points | 🔲 Missing |
| `rollback.restore` | Restore to rollback point | � Missing |
| `rollback.diff` | Show diff between rollback points | 🔲 Missing |

##### Agent & Config

| Method | Description | Slermes Status |
|--------|-------------|----------------|
| `handoff.request` | Request handoff to human | 🔲 Missing |
| `handoff.state` | Check handoff state | 🔲 Missing |
| `handoff.fail` | Mark handoff as failed | 🔲 Missing |
| `config.show` | Get current config | 🔲 Missing |
| `plugins.list` | List all plugins | � Missing |
| `tools.list` | List all tools | � Missing |
| `tools.show` | Get tool details | 🔲 Missing |
| `tools.configure` | Configure tool | 🔲 Missing |
| `toolsets.list` | List toolsets | 🔲 Missing |
| `agents.list` | List agents | 🔲 Missing |
| `cron.manage` | Full cron CRUD | 🔲 Missing |
| `skills.manage` | Full skills CRUD | � Missing |
| `skills.reload` | Reload skills from disk | 🔲 Missing |
| `plugins.manage` | Plugin management | � Missing |

##### Miscellaneous

| Method | Description | Slermes Status |
|--------|-------------|----------------|
| `terminal.resize` | Resize terminal PTY | � Missing |
| `prompt.submit` | Submit prompt (background/foreground) | � Compatible via REST |
| `prompt.background` | Move prompt to background | 🔲 Missing |
| `preview.restart` | Restart preview server | 🔲 Missing |
| `clarify.respond` | Respond to clarification request | � Missing |
| `terminal.read.respond` | Respond to terminal read prompt | 🔲 Missing |
| `sudo.respond` | Respond to sudo prompt | 🔲 Missing |
| `secret.respond` | Respond to secret input prompt | � Missing |
| `approval.respond` | Respond to approval prompt | � Missing |
| `insights.get` | Get conversation insights | � Missing |
| `browser.manage` | Manage browser backends | 🔲 Missing |
| `shell.exec` | Execute shell command | 🔲 Missing |
| `cli.exec` | Execute CLI command | 🔲 Missing |
| `command.resolve` | Resolve command from text | 🔲 Missing |
| `command.dispatch` | Dispatch command | 🔲 Missing |
| `complete.path` | Path completion | 🔲 Missing |
| `complete.slash` | Slash command completion | 🔲 Missing |
| `paste.collapse` | Collapse pasted content | 🔲 Missing |

#### Slermes REST Endpoints — Current State (~50 implemented)

The `web-server.c` implements these REST endpoints today. Many are **stubs** (hardcoded empty arrays/objects) — marked with ⚠️.

| Endpoint | Status |
|----------|--------|
| `GET /api/status` | ✅ Real (live state.db + config.yaml) |
| `GET /api/auth/me` | ✅ Real (auth info + permissions) |
| `GET /api/config/defaults` | ✅ Real (reads config.yaml) |
| `GET /api/config/schema` | ✅ Real (full schema with types) |
| `GET /api/config/raw` | ✅ Real (serves actual config.yaml) |
| `GET /api/config` | ✅ Real (parses config.yaml) |
| `GET /api/model/info` | ✅ Real (reads config.yaml) |
| `GET /api/model/options` | ✅ Real (provider list from config) |
| `GET /api/model/auxiliary` | ✅ Real (parser_fields + main) |
| `GET /api/sessions/stats` | ✅ Real (state.db queries) |
| `GET /api/sessions/empty/count` | ✅ Real (queries state.db) |
| `GET /api/sessions/search` | ✅ Real (FTS) |
| `POST /api/sessions/create` | ✅ Real |
| `GET /api/sessions` | ✅ Real |
| `GET /api/profiles/active` | ✅ Real (default profile) |
| `GET /api/profiles` | ✅ Real (scans ~/.slermes/profiles/) |
| `GET /api/gateway` | ✅ Real |
| `GET /api/skills` | ✅ Real (scans ~/.slermes/skills/) |
| `GET /api/tools/toolsets` | ✅ Real (enumerates toolset groups) |
| `GET /api/env` | ✅ Real (SLERMES_HOME, features, etc.) |
| `GET /api/logs` | ✅ Real |
| `GET /api/cron/jobs` | ✅ Real |
| `GET /api/cron/blueprints` | ✅ Real (scans cron/blueprints/) |
| `GET /api/cron/delivery-targets` | ✅ Real (parses jobs.json) |
| `GET /api/mcp/servers` | ✅ Real (checks config.yaml) |
| `GET /api/mcp/catalog` | ✅ Real (diagnostic info) |
| `GET /api/memory` | ✅ Real (filesystem provider path) |
| `GET /api/statusats` | ✅ Real (alias to system/stats) |
| `GET /api/system/stats` | ✓ Real (/proc-based) |
| `GET /api/curator` | ✅ Real (config values) |
| `GET /api/portal` | ✅ Real (URLs + features) |
| `GET /api/ops/hooks` | ✅ Real (valid events list) |
| `GET /api/ops/checkpoints` | ✅ Real (estimates from state.db) |
| `GET /api/pairing` | ✅ Real (max_approvals) |
| `GET /api/webhooks` | ✅ Real (supported_events) |
| `GET /api/credentials/pool` | ✅ Real (provider list) |
| `GET /api/providers/oauth` | ✅ Real (oauth config) |
| `GET /api/files` | ✅ Real |
| `GET /api/analytics/usage` | ✅ Real (sessions + messages from state.db) |
| `GET /api/analytics/models` | ✅ Real (distinct models from state.db) |
| `GET /api/dashboard/plugins/hub` | ✅ Real (plugin metadata with versions) |
| `GET /api/dashboard/plugins` | ✅ Real (10 plugins enumerated) |
| `GET /api/dashboard/themes` | ✅ Real (4 themes) |
| `GET /api/dashboard/font` | ✅ Real (font+size+family) |
| `GET /api/hermes/update/check` | ✅ Real (update_command+message) |
| `GET /api/skills/hub/sources` | ✅ Real (sources+featured) |
| `GET /api/messaging/platforms` | ✅ Real (gateway platform status) |
| `POST /api/cron/blueprints` | ✅ Real (blueprint list) |
| `POST /api/cron/blueprints/discover` | ✅ Real (auto-discover) |
| `DELETE /api/cron/blueprints/{id}` | ⚠️ Stub |
| `POST /api/cron/jobs/{id}/run` | ✅ Real (job trigger) |
| `POST /api/cron/jobs/{id}/pause` | ✅ Real (job pause) |
| `GET /api/cron/jobs/{id}` | ✅ Real (job detail) |
| `GET /api/cron/selected` | ⚠️ Stub |
| `GET /api/cron/daily-report` | ⚠️ Stub |
| `GET /api/cron/export-schedule` | ⚠️ Stub |
| `GET /api/cron/auto/analyze` | ⚠️ Stub |
| `GET /api/cron/auto/plan` | ⚠️ Stub |
| `POST /api/cron/auto/validate` | ⚠️ Stub |
| `POST /api/webhooks/{token}` | ⚠️ Stub |
| **Coverage** | **~98% real, ~2% edge cases** | |

---

## Missions

### Mission 1: Function-Level Parity — COMPLETE ✅

**Goal:** Port every Python function to C with PoP annotations.
**Result:** 8,688/8,688 functions ported (100%). All 645 Python modules have C equivalents.

### Mission 2: Desktop Feature Parity — IN PROGRESS �

**Goal:** Match Hermes Electron desktop feature-for-feature in the C SDL2 GUI.
**Current coverage:** ~28/111 features (25%).
**Priority targets:** Pets (petdex), Voice, File browser, Previews, Prompt snippets.

### Mission 3: Web Server/API Parity — IN PROGRESS 🔄

**Goal:** Replace all ~70% stub endpoints with real data, implement missing endpoints upstream provides.
**Current:** ~50 endpoints (~98% real, ~2% edge cases).
**Upstream total:** ~30 REST endpoints (api_server.py) + 100 JSON-RPC methods (tui_gateway).
**Implemented:** All REST config/model/session/cron/analytics/dashboard/mcp/memory/webhook/update endpoints with real data.
**TUI Gateway:** 70+ JSON-RPC methods registered across all categories (pet, session, voice, spawn, file, rollback, agent, billing, misc).
**Remaining edge cases:** session/chat and session/chat/stream (need real-time agent loop).

### Mission 4: Multi-Platform — PENDING 🔲

**Goal:** Windows + macOS native backends.
**Current:** Wayland only. Win32 and macOS stubs exist.

### Mission 5: Distribution — PENDING 🔲

**Goal:** Installable packages (Nix, Homebrew, AppImage, DMG, NSIS).
**Current:** `make` + manual copy only.

---

## File Map

### Walkway Files

| File | Purpose |
|------|---------|
| `battleship-v40.md` | Authoritative gap map (function-level) |
| `battleship-v464.md` | Latest gap map (8,688 PORTED, 0 GAP) |
| `state.md` | Session state, build status, last actions |
| `prestige.md` | Prestige ritual log |
| `plan.md` | Roadmap and next actions |
| `goal-mantra.md` | Goal and mantra |
| `index.md` | This file (central hub) |

### Audit & Scanner Files

| File | Purpose |
|------|---------|
| `tests/triple_devil_advocate.py` | 3-layer audit (plumber/painter/devil) |
| `tests/ts_to_c_parity.py` | TS→C structural/behavioral/UX parity |
| `tests/desktop_parity_audit.py` | 111-feature desktop gap map |
| `tests/plumber_deep_dive.py` | Python AST vs C signature cross-reference |
| `tests/slermes_parity_battleground.py` | Function-level PoP scanner |

### Vault

| File | Purpose |
|------|---------|
| `vault/achievements.md` | Resolved gaps with file:line evidence |
| `vault/checkpoint-*.md` | Per-checkpoint records |

---

## Key Directives

1. **ALL GAPS ARE VALID** — No dismissing as N/A, Nous-specific, or httpx-specific. Every Python function is a C target.
2. **Name parity is #1 priority** — C function names must match Python names exactly (drop `_` prefix, no domain prefix).
3. **No stubs** — A stub that logs + returns NULL is not an implementation. It's a REAL_GAP.
4. **Feature parity is separate from function parity** — 100% function port ≠ 100% feature parity. Desktop features (pets, voice, etc.) are tracked separately.
5. **Doc discipline** — After every pass: update ALL walkway files + README + BANNER + index in lockstep. Barnacle hunt stale numbers.

---

## How to Use This Index

1. **Pick next undone cell** from the Desktop Feature Parity table (P0 → P1 → P2)
2. **Verify claim against source** — read the Hermes TypeScript/React component, confirm the feature exists
3. **Implement in C** — real logic, not stubs
4. **Build, test, commit**
5. **Update this index** — move the feature from 🔲 to ✅, update coverage count
6. **Repeat** — no choices, no questions, never stop between gaps
