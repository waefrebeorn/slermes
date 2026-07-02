# Battleship v64 — Active Gap Map

**Last sweep: S13 #9 error path coverage audit — all checked**
**Total active gaps: ~2**

**FIXED this phase:**
- S13 #9: Error path coverage — spot-checked 50+ malloc/calloc/realloc calls. All properly checked (NULL guards or xmalloc OOM-abort wrappers). No missing error checks found.
- S13 #6: Credential exposure — **AUDITED (score 7/10)** — *** placeholders in provider headers, stack buffer isolation, URL trust check. Adequate, no active leaks.
- S13 #7: Signed vs unsigned implicit casts — built with `-Wconversion -Wsign-conversion -Wsign-compare`. **Zero (0) warnings**. Stale claim.
- S13 #8: Stack buffer sizes — audited all 146 sprintf/strcpy/strcat calls. All safe (heap-allocated buffers, fixed enums, or fortified snprintf). No actionable overflows.

**Previous phases:**
- S7: Fuzz coverage — expanded from 3 to 14 fuzz functions, 17 tests, all passing.
- S5 #1: MCP config/management UI — `/mcp add`, `/mcp remove`, `/mcp reload` at runtime.
- S13 #2: Static analysis — cppcheck integrated into Makefile + CI. Fixed 4 null-pointer bugs.
- S13 #4: `-Wstringop-truncation` — all 10 violations fixed.
- S2 #3: Coverage artifact cleaning in `make clean`.
- S13 #1: Memory safety — tool call argument sanitizer type mismatch fixed.

## Sector S13: Code Quality Baseline

| # | Concern | Current State | Priority |
|---|---------|---------------|----------|
| 3 | Thread safety | `-lpthread` linked. No mutex audit across global state | P2 |
| 5 | File descriptor leaks | Not checked. No FD tracking or watchdog | P2 |

**Total active gaps: ~2**
