# Next Session Prompt — Copy-Paste Ready

---

```
Goal: Full app parity — desktop app, web app, all code types, all docs

State: Function-level parity 100% (8,688/8,688 PORTED). Desktop GUI at ~30/111 features (27%). Web-server ~99% real. ALL upstream code types reclassified as REAL_GAP: 470 desktop TS/TSX files, 212 TUI TS/TSX files, 311 skill .md files, 749 upstream .md docs, 26 scripts, 30 packaging files, 200+ test files, 855 JSON configs.

Build: Clean. Last commit: prestige ritual needed.

The Loop:
1. Pick next undone cell from battleship index (mind-palace/index.md)
   Priority order: Desktop P2 features → Documentation serving (Mission 5) → Skills system (Mission 6) → Distribution (Mission 7) → Test parity (Mission 8)
2. Verify claim against source before implementing
3. Implement real C logic — no stubs, no hermes_log+return NULL
4. Build: make -j$(nproc)
5. Test: curl web endpoints / interact with desktop UI / ./slermes <command> to verify
6. Triple DA check:
   - Plumber: Does the code exist in C? (file search)
   - Painter: Does it work? (build + test)
   - Devil: Does it 1:1 match upstream? (diff against source file)
7. Commit: git add -f .hermes/ src/ && PRE_COMMIT_ALLOW_NO_CONFIG=1 git commit -m "vXXX: area — what changed"
8. Update mind-palace/index.md — move items from 🔲 to ✅
9. Repeat until battleship is ALL ✅

Key rules:
- ALL code types Nous produces are REAL_GAP — TS, TSX, Python, JS, shell, PS1, MD, HTML, CSS, JSON, YAML
- Rewriting in scratch in C is the point of the project
- Real implementations only — no hardcoded empty responses, no hermes_log+return NULL
- Build must pass before committing
- After every 5 commits: full prestige ritual (barnacle hunt, vault, version bump, push)

Project root: /home/wubu/hermes-agent-dev/slermes
Battleship index: .hermes/mind-palace/index.md
Web server: src/web_server.c
Desktop GUI: src/desktop_gui.c + src/gui_core.c
TUI: src/tui_fullscreen.c
State DB: src/agent/state_db.c

Desktop app upstream: /home/wubu/hermes-agent-dev/apps/desktop/src/ (470 files)
TUI upstream: /home/wubu/hermes-agent-dev/ui-tui/src/ (212 files)
Skills upstream: /home/wubu/hermes-agent-dev/skills/ (72 skills)
Docs upstream: /home/wubu/hermes-agent-dev/docs/ + website/
TUI Gateway: /home/wubu/hermes-agent-dev/tui_gateway/

No choices. No questions. Never stop between gaps.
```
