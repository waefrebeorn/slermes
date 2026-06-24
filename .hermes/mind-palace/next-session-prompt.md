# Next Session Prompt — Copy-Paste Ready

---

```
Goal: Web Server Parity — Replace stubs with real data

State: Function-level parity 100% (8,688/8,688 PORTED). Web-server.c has ~50 endpoints, ~70% are stubs (hardcoded empty responses). Desktop GUI at ~14% feature parity (15/111 features).

Build: Clean. Last commit: 706c5e2 on main.

The Loop:
1. Read web_server.c endpoint list (lines 554-602)
2. For each ⚠️ stub endpoint, replace with real data:
   - /api/config → read ~/.slermes/config.yaml, return actual config
   - /api/model/info → query loaded model context (d_model, n_layers, etc)
   - /api/jobs → read cron/jobs.json, return real job list
   - /api/jobs/{id}/run → trigger actual cron job execution
   - /api/jobs/{id}/pause → pause running job
   - /api/sessions/{id}/chat → wire to real agent loop
   - /api/sessions/{id}/chat/stream → SSE streaming from agent
   - /api/sessions/{id}/fork → clone session in state.db
   - /api/sessions/{id}/messages → return real message history
   - /api/auth/me → return actual provider key status (masked)
   - /api/providers/oauth → return real OAuth provider status
   - /api/analytics/usage → compute real token usage from session data
   - /api/dashboard/themes → scan theme directory, return real list
   - /api/dashboard/font → return actual font config
   - /api/hermes/update/check → compare local vs remote version
3. Build: make -j$(nproc)
4. Test: curl each endpoint to verify real data returned
5. Commit: git add -f src/web_server.c .hermes/ && PRE_COMMIT_ALLOW_NO_CONFIG=1 git commit -m "vXXX: web server — N endpoints real (was stubs)"
6. Repeat until all stubs replaced

Key rules:
- Real data only — no hardcoded empty arrays/objects
- Read from live sources: state.db, ~/.slermes/config.yaml, cron/jobs.json, /proc/
- Build must pass before committing
- Update .hermes/mind-palace/index.md — move endpoints from ⚠️ to ✅
- On final commit: full prestige ritual (barnacle hunt, vault, version bump)

Project root: /home/wubu/hermes-agent-dev/slermes
Web server source: src/web_server.c
Web dashboard source: src/web_dashboard.c
State DB: src/agent/state_db.c

Priority order (easiest wins first):
1. /api/config* → read config.yaml (libyaml already linked)
2. /api/jobs* → read cron/jobs.json (jansson already linked)
3. /api/analytics* → compute from session stats
4. /api/dashboard* → scan filesystem
5. /api/auth/me → check key file existence
6. /api/sessions/{id}/fork → state.db clone
7. /api/sessions/{id}/chat → agent loop integration (hardest)

No choices. No questions. Never stop between endpoints.
```
