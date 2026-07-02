# Battleship v63 — Active Gap Map

**Last sweep: S13 #7 (signed/unsigned) + S13 #8 (stack buffers) verified clean**
**Total active gaps: ~4**

**FIXED this phase:**
- S13 #7: Signed vs unsigned implicit casts — built with `-Wconversion -Wsign-conversion -Wsign-compare`. **Zero (0) warnings** across entire codebase. Claim was stale.
- S13 #8: Stack buffer sizes — audited all 146 sprintf/strcpy/strcat calls. All safe (heap-allocated buffers, fixed enums, or fortified snprintf). No actionable overflows.

**Previous phases:**
- S7: Fuzz coverage — expanded from 3 to 14 fuzz functions, 17 tests, all passing.
- S5 #1: MCP config/management UI — `/mcp add`, `/mcp remove`, `/mcp reload` with runtime server connection and tool registration.
- S13 #2: Static analysis — `make static-analysis` target with cppcheck 2.17.1, integrated into CI workflow. Fixed 4 null-pointer bugs.
- S13 #4: `-Wstringop-truncation` warning — all 10 violations fixed (memcpy + explicit null).
- S2 #3: Coverage artifact cleaning — `make clean` now removes .gcda/.gcno and coverage report dirs.
- S13 #1: Memory safety — tool call argument sanitizer type mismatch fixed.
- test_runner: timeout wrappers, slash-in-filename bug, mangled redirect, wait before integration, webhook curl timeout

## Sector S13: Code Quality Baseline

| # | Concern | Current State | Priority |
|---|---------|---------------|----------|
| 3 | Thread safety | `-lpthread` linked. No mutex audit across global state | P2 |
| 5 | File descriptor leaks | Not checked. No FD tracking or watchdog | P2 |
| 6 | Credential exposure | **AUDITED** — `***` placeholder in provider headers, stack buffer isolation, URL trust check. Score 7/10 — adequate, no active leaks. | P1 |
| 9 | Error path coverage | Most functions return int. No audit that error paths checked | P2 |

**Total active gaps: ~4**
