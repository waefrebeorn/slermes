# Battleship v62 — Active Gap Map

**Last sweep: S7 fuzz coverage (14 fuzz functions, 17 tests, 0 failures)**
**Total active gaps: ~6**

**FIXED this phase:**
- S7: Fuzz coverage — expanded from 3 fuzz functions to 14 (JSON random binary, tool args, paths, YAML, multi-doc YAML, dotenv, cron, host:port, URL encoding, proxy bypass, deeply nested JSON, YAML→JSON, dotenv iteration, JSON5 edge cases). 17 total test checkpoints, 1000+ random JSON iterations. All pass. Builds via `make fuzz`.

**Previous phases:**
- S5 #1: MCP config/management UI — `/mcp add`, `/mcp remove`, `/mcp reload` with runtime server connection and tool registration.
- S13 #2: Static analysis — `make static-analysis` target with cppcheck 2.17.1, integrated into CI workflow. Fixed 4 null-pointer bugs (secrets.c, web_search_registry.c, resource.c, agent_loop.c). **0 errors, 5 baseline warnings.**
- S13 #4: `-Wstringop-truncation` warning — all 10 violations fixed (memcpy + explicit null). Also cleaned up unused `message_deep_copy`, missing `ctype.h`.
- S2 #3: Coverage artifact cleaning — `make clean` now removes .gcda/.gcno and coverage report dirs.
- **Stale claims verified:** S0a #3 (onboarding welcome), #5 (debug upload), S11 #1 (CI), #2 (cov gate), #3 (ASan), S5 #2 (plugin install/remove), S2 #4 (parallel phony targets) — all already implemented. S12 all CONFIRMED.
- S13 #1: Memory safety — `hermes_sanitize_tool_call_arguments()` called with `message_t**` from agent_loop but expected `message_t*`. Fixed with temp contiguous buffer + copy-back.
- test_runner: timeout wrappers, slash-in-filename bug, mangled redirect, wait before integration, webhook curl timeout

## Sector S13: Code Quality Baseline

| # | Concern | Current State | Priority |
|---|---------|---------------|----------|
| 3 | Thread safety | `-lpthread` linked. No mutex audit across global state | P2 |
| 5 | File descriptor leaks | Not checked. No FD tracking or watchdog | P2 |
| 6 | Credential exposure | **AUDITED** — `***` placeholder in provider headers, stack buffer isolation, URL trust check. Score 7/10 — adequate, no active leaks. | P1 |
| 7 | Signed vs unsigned | Warnings visible. Implicit casts in library code | P2 |
| 8 | Stack buffer sizes | `char buf[4096]` patterns. No snprintf bounds audit | P2 |
| 9 | Error path coverage | Most functions return int. No audit that error paths checked | P2 |
| 10 | Fuzzer on JSON/YAML | **DONE** — 14 fuzz functions, 17 tests, JSON/YAML/JSON5/dotenv/cron/path/http covered. `make fuzz` passes with 0 failures. | P2 |

## Sector S7: Fuzz Coverage

**COMPLETED — All 14 fuzz functions implemented and passing.**
- 14 fuzz functions covering: JSON binary, tool args, paths, YAML, multi-doc YAML, dotenv, cron, host:port, URL encoding, proxy bypass, deeply nested JSON, YAML→JSON serialization, dotenv iteration, JSON5 edge cases
- 17 total test checkpoints, 1000+ random JSON iterations
- Run with: `make fuzz && ./slermes-fuzz`

**Total active gaps: ~6**
