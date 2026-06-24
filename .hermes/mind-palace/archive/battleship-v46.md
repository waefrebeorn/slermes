# Battleship v46 — Active Gap Map (DA-Corrected)

**Last sweep: Phase 572 — S13 Null safety**  
**Total active gaps: ~79** (S13 #2 Null safety FIXED v572)

**FIXED this phase:**
- S13 #2 Null safety: NULL handler check in registry_dispatch (both dispatch paths). 10 lines in src/tools/registry.c.

**FIXED previous:**
- S12 MCP integration E2E: new `test_mcp_integration.c` with 44 assertions across 5 phases
- S12 Plugin lifecycle E2E: new `test_plugin_e2e.c` with 41 assertions across 6 lifecycle phases
- S11 #1: GitHub Actions CI — `.github/workflows/ci.yml` added (build + test + asan on push/PR)
- S0a #2: `/key` command — .env key wizard, 4 subcommands (list, get, set, delete)
- Tools: 105→86 unique (was counting registrations, not unique tools)
- CLI commands: 98→88 (was including subcommand generics)
- Benchmarks: "ACHIEVED 100%"→ ⚠️ COMPILATION FAILED → FIXED (missing dotenv include)
- Fuzz: 51→52 functions

---

## Sector S0a: Setup & UX (2 gaps)
Interactive setup wizard ✅ (v565), .env key wizard ✅ (v570), onboarding ✅ (v576), debug upload ✅ (v577), uninstall (P2), backup/restore (P3).

## Sector S0b: Install & Distribution (2 gaps)
Nix flake (P3), Homebrew formula (P3). ~~Docker image CI~~ ✅ (v564).

## Sector S1: Agent Loop (2 gaps)
Memory system depth — external providers (P1), onboarding UI wiring (P2).

## Sector S2: Build Portability (2 gaps)
Parallel compilation (P3), pre-commit hook verification (P3).

## Sector S3: CLI Commands (corrected)
**88 slash commands** (was 98 — corrected in DA). PORTED ≥95%.

## Sector S4: Gateway Platforms — PARTIAL ~30%
19 platforms compile. Gateway protocol/session layer PARTIAL.

## Sector S5: Tool Depth (1 gap)
Browser CDP depth ~~P2~~ ✅ — was missing url_safety.c link in test build (FIXED v579).

## Sector S6: Test Coverage
286/1/21. Benchmark ✅.

## Sector S7: Fuzz Coverage
**52 functions** (was 51 — corrected).

## Sector S8: Benchmark Parity
**✅ WORKING.** 1 test file (test_benchmark.c), 102 assertions. Compilation was broken — missing include paths for dotenv, textwrap, fuzzy_match, glob, ansi, datetime, binary, difflib, template, interrupt. All FIXED in DA v1 (v566).

## Sector S9: Dead Code
ALL active items known. Intentionally unused functions remain (13 items).

## Sector S10: LOC Ratio
C core ~62K (agent+tools) vs Python ~125K (49%). Total C ~129K.

## Sector S11: Test Infrastructure (2 gaps)
GitHub Actions CI ✅ (ci.yml added v568), Coverage gate (P3), ASan CI (P3).

## Sector S12: E2E Verification — **ALL DONE ✅**
**VERIFIED:** U01 (full conv), U08 (HTTP), #2 (gateway webhook), ~~#6 (kanban)~~ ✅, ~~plugin lifecycle~~ ✅ (41 assertions, 6 phases), ~~MCP integration~~ ✅ (44 assertions, 5 phases, 7 tools), ~~Cron~~ ✅ (54 assertions, 6 phases, cron_should_run impl), ~~Browser CDP~~ ✅ (43 assertions, 6 phases).
**FIXED:** U03 (user profile).
**S12: ALL 8 E2E GAPS CLOSED.**

## Sector S13: Code Quality
**FIXED:** C01 (ASan), C04 (stringop truncation), C06 (credential audit), ~~C13 #2 Null safety~~ ✅ (v572).
**VERIFIED STALE:** signed vs unsigned — 0 warnings across codebase.
**REMAINING:** Thread safety (P2), FD leaks (P2), stack buf audit (P2), error path coverage (P2), fuzz corpus (P2).

## Sector S14: Python Source Comparison — 10/10 COMPLETE
U01 ✅ 85%, U02 ✅ 40%, U03 ✅ 30%, U04 ✅ 25%, U05 ✅ 90%, U06 ✅ 65%, U07 ✅ 75%, U08 ✅ 70%, U09 ✅ 75%, U10 ✅ 90%.

## Sector S15: Dead Methodology (6 items)
Confirmed. 4/10 S14 items complete. C = skeleton, Python = 2-3x more logic.

---

**LEGEND:** PORTED (≥80%) / PARTIAL (20-80%) / REAL GAP (<20%)
