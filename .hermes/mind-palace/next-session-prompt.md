# Next Session Prompt — Copy-Paste Ready

---

```
Goal: Full app parity — desktop app, web app, all endpoints

State: Function-level parity 100% (8,688/8,688 PORTED). Desktop GUI at ~14% feature parity. Web-server ~30% real, ~70% stubs.

Build: Clean. Last commit: 706c5e2 on main.

The Loop:
1. Pick next undone cell from battleship index (mind-palace/index.md)
2. Verify claim against source before implementing
3. Implement real C logic — no stubs, no hermes_log+return NULL
4. Build: make -j$(nproc)
5. Test: curl web endpoints / screenshot desktop GUI to verify
6. Commit: git add -f .hermes/ src/ && PRE_COMMIT_ALLOW_NO_CONFIG=1 git commit -m "vXXX: area — what changed"
7. Repeat. Desktop → Web → Infrastructure. No ordering within priority.

Key rules:
- Real data only — no hardcoded empty responses
- Build must pass before committing
- Update mind-palace/index.md — move items from 🔲 to ✅
- On final commit: full prestige ritual (barnacle hunt, vault, version bump)

Project root: /home/wubu/hermes-agent-dev/slermes
Battleship index: .hermes/mind-palace/index.md
Web server: src/web_server.c
Desktop GUI: src/desktop_gui.c + src/gui_core.c

No choices. No questions. Never stop between gaps.
```
