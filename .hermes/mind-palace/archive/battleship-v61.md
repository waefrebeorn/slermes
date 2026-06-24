# Battleship v61 — Active Gap Map

**Last sweep: S5 #1 MCP management + S13 #2 cppcheck**
**Total active gaps: ~7**

**FIXED this phase:**
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
| 1 | Memory safety | **FIXED** — webhook POST no longer SEGVs. Tool call arg sanitizer type mismatch resolved. | P0 |
| 2 | Null pointer safety | **DONE** — cppcheck in Makefile + CI, 0 errors found | P1 |
| 3 | Thread safety | `-lpthread` linked. No mutex audit across global state | P2 |
| 4 | Buffer overflow | `-Wstringop-truncation` enabled — **ALL 10 WARNINGS FIXED** | P1 |
| 5 | File descriptor leaks | Not checked. No FD tracking or watchdog | P2 |
| 6 | Credential exposure | **AUDITED** — `***` placeholder in provider headers, stack buffer isolation, URL trust check. Score 7/10 — adequate, no active leaks. | P1 |
| 7 | Signed vs unsigned | Warnings visible. Implicit casts in library code | P2 |
| 8 | Stack buffer sizes | `char buf[4096]` patterns. No snprintf bounds audit | P2 |
| 9 | Error path coverage | Most functions return int. No audit that error paths checked | P2 |
| 10 | Fuzzer on JSON/YAML | Fuzz harness exists. No fuzz corpus for malformed config | P2 |

## Sector S7: Fuzz Coverage

52/62 fuzz functions implemented (84%), 180+ active fuzz assertions. 10 remaining.

**Total active gaps: ~7**
