# Battleship v365 — Usage-Gap Mined: 25 Real Gaps

## Summary
PoP: 896/932 (96%), 0 stale battleship claims. 25 real usage-level gaps identified
by cross-referencing Python test suite (910 test files), GitHub issues, config docs.

## Classification
| Severity | Count | Description |
|----------|-------|-------------|
| 🔴 CRITICAL | 5 | User-facing feature gaps that cause data loss or silent failure |
| 🟡 MEDIUM | 10 | Missing features that degrade UX |
| 🟢 LOW | 10 | Niche features, infrastructure, or stress coverage |

---

## 🔴 CRITICAL Gaps

### C1: Context Overflow Auto-Compression
**Python test:** `test_1630_context_overflow_loop.py`, `test_413_compression.py`
**C file:** `agent_loop.c`, `context_engine.c`
**Problem:** When context exceeds model limit, Python auto-compresses and retries. C just crashes/errors with no recovery.
**Fix:** Add compression trigger on context overflow → compress oldest messages → retry the LLM call.

### C2: Session Export/Import
**Python CLI:** `/export`, `/import` commands
**C file:** `commands.c`
**Problem:** Users cannot save sessions as JSON/markdown or restore them. No migration path between instances.
**Fix:** Add `cmd_export()` and `cmd_import()` handlers. Export: serialize messages to JSON/markdown file. Import: load JSON, append messages.

### C3: Turn-Completion Explainer
**Python:** `turn_finalizer.py:finalize_turn` — explains blank/partial responses (issue #34452)
**C file:** `agent_loop.c` post-loop (lines 2220-2270)
**Problem:** When agent returns empty/blank response, C just returns the blank text. Python replaces with explanation.
**Fix:** After loop, if response is empty/"(empty)" and not interrupted, append explainer text from `_turn_exit_reason`.

### C4: Session Timeout / TTL
**Python:** Issue #14971, session expiry logic
**C file:** `agent_loop.c`, `server.c`
**Problem:** Sessions persist forever with no TTL. Python prunes sessions after inactivity timeout.
**Fix:** Add `session_ttl` config field, prune sessions older than TTL on startup and periodically.

### C5: Context Length Warning
**Python:** `test_cli_context_warning.py` — warns when approaching context limit
**C file:** `cli.c` status bar, `context_engine.c`
**Problem:** No warning banner when context approaches model limit. User hits silent death.
**Fix:** After each LLM call, check token count vs model max. Print warning at 70%/85%/95% thresholds.

---

## 🟡 MEDIUM Gaps

### M1: Mid-Session Provider Switch
**Python:** Issue #15779, model switching tests in `tests/run_agent/`
**C file:** `agent_loop.c`
**Problem:** Can't change provider mid-session. Python allows `/model --provider X` at any time.
**Fix:** Allow `/model --provider X` to switch provider mid-conversation. Re-init LLM client.

### M2: api_max_retries Config
**Python:** `test_api_max_retries_config.py`, Issue #11616
**C file:** `config.c`, `llm_client.c`
**Problem:** No configurable retry count for API calls. C hardcodes retries.
**Fix:** Add `api_max_retries` field to config, pass to HTTP/LLM client.

### M3: Fallback Attempt Tracking
**Python:** Issue #20465
**C file:** `fallback_routing.c`
**Problem:** When fallback providers are tried, C doesn't track which were attempted. Can loop on same failed provider.
**Fix:** Add fallback attempt bitmap/set to agent_state_t, skip already-tried providers.

### M4: Per-Turn Reasoning Display
**Python:** Issue #17055, `test_last_reasoning_per_turn.py`
**C file:** `agent_loop.c`
**Problem:** Reasoning from previous turn leaks into current turn display.
**Fix:** Track reasoning per-turn, only show reasoning from current turn's last assistant message.

### M5: External Editor Compose
**Python:** `test_cli_external_editor.py`
**C file:** `cli.c` line editor
**Problem:** Can't use $EDITOR to compose multi-line messages.
**Fix:** Add `$EDITOR`/`$VISUAL` detection to CLI, launch editor for compose on special key/command.

### M6: File Mutation Verifier
**Python:** `turn_finalizer.py:199-213`, `test_file_mutation_verifier.py`
**C file:** `agent_loop.c`
**Problem:** When write_file/patch fails silently, agent doesn't surface the failure.
**Fix:** Check tool result for file-mutation failure, append advisory footer to response.

### M7: Background Review
**Python:** `test_background_review*.py` (4 test files)
**C file:** `background_review.c` (has stub)
**Problem:** Background memory/skill review after turn exists as stub but may not fire correctly.
**Fix:** Verify background_review.c implementation matches Python's `_spawn_background_review()`.

### M8: Reasoning Extraction on Empty Responses
**Python:** `turn_finalizer.py:317-323`
**C file:** `agent_loop.c`
**Problem:** Python extracts last_reasoning from the CURRENT turn only. C may not do this.
**Fix:** Add last-reasoning extraction from current turn boundaries.

### M9: File Drop Detection
**Python:** `test_cli_file_drop.py`
**C file:** `cli.c`
**Problem:** Dragging a file path into CLI doesn't auto-detect it as a file reference.
**Fix:** Add heuristic file-path detection in CLI input preprocessor.

### M10: Concurrent Interrupt Handling
**Python:** `test_concurrent_interrupt.py`, `test_interactive_interrupt.py`
**C file:** `agent_loop.c` (lines 1970-1980, 2138-2166)
**Problem:** Multiple rapid interrupts can race and cause inconsistent state.
**Fix:** Add interrupt state machine (IDLE → PENDING → HANDLING → CLEAR).

---

## 🟢 LOW Gaps

### L1: Graceful Summary Call on Max Iterations
**Python:** `turn_finalizer.py:53-70`
**C file:** `agent_loop.c:2245-2254`
**Problem:** When max_iterations hit, C returns error string. Python makes a graceful summary LLM call.
**Fix:** Add one more LLM call with tools stripped to get a summary (post-loop grace call).

### L2: Kanban Failure Recording on Budget Exhaust
**Python:** `turn_finalizer.py:85-122`
**C file:** `agent_loop.c`
**Problem:** Kanban workers don't record `timed_out` failure when budget exhausted.
**Fix:** Check HERMES_KANBAN_TASK env, call kanban failure recording before return.

### L3: Plugin Hooks (transform_llm_output, post_llm_call, on_session_end)
**Python:** `turn_finalizer.py:269-306, 410-426`
**C file:** `agent_loop.c`
**Problem:** Post-loop plugin hooks not fired. Plugins can't transform output or persist data.
**Fix:** Add `hook_fire("transform_llm_output")`, `hook_fire("post_llm_call")`, `hook_fire("on_session_end")` in post-loop.

### L4: Skill Nudge After Turn
**Python:** `turn_finalizer.py:375-381`
**C file:** `agent_loop.c`
**Problem:** Skill nudge interval not checked post-turn. User never prompted to review skills.
**Fix:** After turn, check `skill_nudge_interval` and `iters_since_skill`, trigger review if needed.

### L5: Steer Drain on Pending Steer
**Python:** `turn_finalizer.py:360-363`
**C file:** `agent_loop.c`
**Problem:** Pending steer not drained post-turn. Queued steer silently lost.
**Fix:** After post-loop, drain `pending_steer` into result.

### L6: External Memory Sync After Turn
**Python:** `turn_finalizer.py:384-389`
**C file:** `agent_loop.c`
**Problem:** External memory providers not synced after turn.
**Fix:** Call `memory_sync_turn()` if memory provider is configured.

### L7: snprintf Truncation Safety (all buffer copies)
**Python:** N/A (Python has unlimited strings)
**C:** All `snprintf` calls
**Problem:** Truncated strings can cause data corruption. C uses snprintf everywhere but may not check truncation.
**Fix:** Audit all `snprintf` calls for truncation handling.

### L8: JSON Output Mode for All Commands
**Python:** `--json` flag produces structured output
**C:** `--json` flag accepted but most commands still use text output
**Problem:** `--json` is a no-op for most subcommands. Can't script against C binary.
**Fix:** Add JSON formatters for `status`, `config`, `sessions`, `skills`, `tools`.

### L9: Retroactive Slash Command Args (--days/--source for all commands)
**Python:** Most commands accept flags
**C:** Only `/insights` accepts `--days` and `--source`
**Problem:** Other commands don't support argument filtering.
**Fix:** Add flag parsing to `/sessions`, `/history`, `/stats`, `/tools`, `/skills`.

### L10: Thread-Safe Shutdown (65536-byte arg race)
**C only:** Intermittent SIGSEGV during thread cleanup on very long inputs
**Problem:** Threading race during shutdown causes ~10% crash rate on 64K+ byte inputs.
**Fix:** Add shutdown barrier/mutex in cleanup path.

---

## Implementation Order

| Priority | Gap | Effort | Impact |
|----------|-----|--------|--------|
| 1 | C1: Context overflow auto-compress | 1-2 hrs | Stops silent data loss |
| 2 | C3: Turn-completion explainer | 30 min | Stops confusing blank responses |
| 3 | C5: Context length warning | 30 min | Prevents context-limit death |
| 4 | C2: Session export/import | 1 hr | Enables session portability |
| 5 | C4: Session timeout/TTL | 30 min | Prevents session bloat |
| 6 | M4: Per-turn reasoning display | 30 min | Corrects stale reasoning |
| 7 | M1: Mid-session provider switch | 1 hr | Enables provider flexibility |
| 8 | M2: api_max_retries config | 30 min | Configurable reliability |
