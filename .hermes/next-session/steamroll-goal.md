# Next Session: Full App Parity — Steamroll All Remaining Missions

## Goal: Complete Missions 5-8. Full app parity across ALL code types.

## State at Start
- **Version:** v500, pushed to origin/main
- **Build:** Clean, 0 errors (desktop-gui + slermes + web-server)
- **Function parity:** 8,688/8,688 (100%) ✅
- **Desktop:** 95/111 features (100% actionable) ✅
- **Web API:** ~50 REST endpoints (~99% real) ✅
- **Platform backends:** Linux + Win32 + macOS ✅
- **Mission 5:** Route stubs added to web_server.c (`/api/docs`, `/api/docs/architecture`, `/api/docs/contributing`, `/api/docs/readme`) — handlers NOT IMPLEMENTED yet (declarations only, calls will segfault)
- **Missions 6-8:** Not started

## The Missions (Priority Order)

### Mission 5: Documentation & Web Content → NEXT
**Goal:** Serve key upstream documentation through web_server.c
**Scope:**
1. Implement the 4 declared handler bodies (`h_docs`, `h_docs_architecture`, `h_docs_contributing`, `h_docs_readme`) in `src/web_server.c`
2. Serve README.md, DESIGN.md, architecture docs as HTML-converted responses
3. Add `/api/docs/*` wildcard route for serving all embedded doc pages
4. Embed key .md files as static char arrays in the binary (or load from disk at startup)
**Upstream source:** `/home/wubu/hermes-agent-dev/docs/` + `/home/wubu/hermes-agent-dev/website/docs/` (670 .md files)
**Priority doc files to embed:**
- README.md (root)
- DESIGN.md
- docs/session-lifecycle.md
- docs/security/ (all)
- docs/design/ (all)
- docs/kanban/ (all)
- website/docs/ (top-level pages: intro, getting-started, installation)

### Mission 6: Skills System
**Goal:** C-side skill loader + port all 72 upstream skills
**Scope:**
1. Implement skill parser for SKILL.md format in C
2. Create `src/skills/` directory with skill loader
3. Port 72 skill definitions from upstream `/home/wubu/hermes-agent-dev/skills/`
4. Add `/api/skills` endpoint to list/execute skills via web server
5. Skills should be parseable executable (display instructions, not actually run shell commands for safety)
**Upstream source:** `/home/wubu/hermes-agent-dev/skills/` (72 skill directories with SKILL.md)

### Mission 7: Distribution
**Goal:** Installable packages for all platforms
**Scope:**
1. AppImage build script (Linux)
2. Homebrew formula (macOS)
3. NSIS installer config (Windows)
4. Docker multi-stage build
5. Nix flake
6. Makefile `install` target that works properly
**Key files:** Create `packaging/` directory with all platform configs

### Mission 8: Test Parity
**Goal:** C-side tests for UI features
**Scope:**
1. Desktop GUI test harness (test desktop_gui.c rendering pipeline)
2. Web endpoint integration tests (curl-based test script)
3. CLI command tests (test all hermes_cli_* commands return correct output)
4. State DB tests (test CRUD operations)
5. Create `tests/ui/` directory with test files

## Execution Rules

1. **Pick next undone cell** — Mission 5 → 6 → 7 → 8, in that order
2. **Verify claim against source** — read the upstream file BEFORE implementing
3. **Real implementations only** — no `hermes_log("TODO"); return NULL;` stubs
4. **Build must pass** before committing: `make -j$(nproc)`
5. **Test:** curl web endpoints, interact with desktop UI, run `./slermes <command>` to verify
6. **Triple DA check** before marking done:
   - Plumber: Does the code exist in C? (grep/glob)
   - Painter: Does it work? (build + curl/test)
   - Devil: Does it 1:1 match upstream? (diff against source)
7. **Commit** after each meaningful unit: `git add -f .hermes/ src/ && PRE_COMMIT_ALLOW_NO_CONFIG=1 git commit -m "vXXX: area — what changed"`
8. **Update battleship index** — move items from 🔲 to ✅
9. **Prestige ritual** every ~5 commits: barnacle hunt, vault, version bump, push

## Key Project Info
- **Project root:** `/home/wubu/hermes-agent-dev/slermes`
- **Battleship index:** `.hermes/mind-palace/index.md`
- **Web server:** `src/web_server.c`
- **Desktop GUI:** `src/desktop_gui.c` + `src/gui_core.c`
- **TUI:** `src/tui_fullscreen.c`
- **State DB:** `src/agent/state_db.c`
- **Upstream docs:** `/home/wubu/hermes-agent-dev/docs/` + `/home/wubu/hermes-agent-dev/website/`
- **Upstream skills:** `/home/wubu/hermes-agent-dev/skills/`
- **Upstream desktop:** `/home/wubu/hermes-agent-dev/apps/desktop/src/`
- **Branch:** main
- **Current version:** v500

## What "Done" Looks Like
- Mission 5: All 4 `/api/docs/*` endpoints return real HTML content from embedded docs
- Mission 6: `/api/skills` returns JSON list of all 72 skills with parsed metadata
- Mission 7: `make appimage` / `make homebrew` / `make nsis` all produce working packages
- Mission 8: `make test` runs UI + endpoint + CLI tests, all pass

## Rules
- No choices. No questions. Never stop between gaps.
- ALL upstream code types are REAL_GAP until slermed to C
- Real implementations only — no hardcoded empty responses
- Build must pass before committing
- After every 5 commits: full prestige ritual
