# Stale Claims Archive — Resolved During Verification

This file archives claims from the battleship that were discovered to be incorrect during function-level verification. Each entry explains what was claimed, what was actually found, and the correct classification.

## Checkpoint 18 Reclassifications

### TD17 `browser_cdp_tool.py` — Claimed: REAL GAP → Actual: ✅ PORTED
- **Stale claim:** No C equivalent for CDP protocol browser tool
- **Reality:** `src/tools/browser.c:1455-1481` implements `browser_cdp_handler()` which sends arbitrary CDP commands via WebSocket. Registered at `browser.c:1750-1753` via `registry_init_browser()`.
- **Root cause:** The battleship tracked Python file names, not function-level capabilities. The C implementation uses a different architecture (single browser.c file with multiple handlers) vs Python's separate tool files.

### TD18 `browser_dialog_tool.py` — Claimed: REAL GAP → Actual: ✅ PORTED
- **Stale claim:** No C equivalent for browser dialog handling
- **Reality:** `src/tools/browser.c:1621-1650` implements `browser_dialog_handler()` for JavaScript dialogs (accept/dismiss/prompt). Registered at `browser.c:1745-1748`.
- **Root cause:** Same as TD17 — different file organization in C vs Python.

### PL18 `gateway_platforms` plugin — Claimed: REAL GAP → Actual: N/A
- **Stale claim:** No gateway platforms plugin in C
- **Reality:** Python `plugins/platforms/` contains 8 thin wrapper plugins (discord, google_chat, irc, line, mattermost, ntfy, simplex, teams) that just call `register` from adapters. The actual platform implementations are built-in to the C gateway at `src/gateway/platforms/`. The plugin wrapper pattern is Python-specific — C doesn't need it because platforms are compiled in.
- **Root cause:** Confusing "plugin wrapper" with "platform implementation." The wrappers add no functionality over the built-in C platforms.

### AL07 `codex_runtime` — Claimed: REAL GAP → Actual: ⚠️ PARTIAL
- **Stale claim:** No codex mode in agent loop
- **Reality:** `src/agent/provider_codex_responses.c` implements a full Responses API provider. `llm_client.c` detects `api_mode=="codex_responses"` and swaps provider ops. The core URL, request format, response parsing, and streaming all work.
- **Classification:** PARTIAL (~60%) because multimodal content conversion, encrypted reasoning replay, cross-issuer guard, and message phase replay are not yet implemented from the 1260-line Python adapter.
- **Root cause:** The config field `codex_runtime` existed but was never wired. The battleship checked for the config field but not for the actual implementation.

### MS11 `codex_responses_adapter.py` — Claimed: REAL GAP → Actual: ⚠️ PARTIAL
- **Stale claim:** No C equivalent for codex responses adapter
- **Reality:** Same as AL07 — `provider_codex_responses.c` covers the core functionality.
- **Root cause:** MS11 and AL07 are the same feature tracked in different sectors (MS = missing subsystems, AL = agent loop).

## Pattern Summary
**5 stale claims resolved in checkpoint 18:**
- 3 were actually PORTED (TD17, TD18, and the platform implementations underlying PL18)
- 1 was N/A (PL18 plugin wrappers)
- 1 was PARTIAL not REAL GAP (AL07/MS11)

**Key lesson:** Always verify against source code, not file-name matching. The C codebase uses different file organization than Python — multiple related tools are often in one .c file rather than separate files.
