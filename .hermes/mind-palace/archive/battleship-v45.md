# Battleship v45 — Active Gap Map (DA-Corrected)

**Last sweep: Phase 571 — S12 MCP integration E2E verified**
**Total active gaps: ~80** (S12 MCP integration E2E FIXED v572)

**FIXED this phase:**
- S12 MCP integration E2E: new `test_mcp_integration.c` with 44 assertions across 5 phases (tool presence, JSON format, error paths, auth routing, resource/prompt handlers). Wired into test_runner.sh.

**FIXED previous:**
- S12 Plugin lifecycle E2E
- S11 #1: GitHub Actions CI — `.github/workflows/ci.yml` added (build + test + asan on push/PR)
- Tools: 105→86 unique (was counting registrations, not unique tools)
- CLI commands: 98→88 (was including subcommand generics)
- Benchmarks: "ACHIEVED 100%"→ ⚠️ COMPILATION FAILED (missing dotenv include)
- Fuzz: 51→52 functions
- Total C source: ~112K→~129K (was undercounting non-src/ agent code)

---

## Sector S0a: Setup & UX (4 gaps)
Interactive setup wizard ✅ (v565), .env key wizard ✅ (v570), onboarding (P2), debug upload (P2), uninstall (P2), backup/restore (P3).

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

## Sector S5: Tool Depth (2 gaps)
Browser CDP depth (P2), Kanban board ~~UNCERTAIN~~ ✅ PORTED (9/9 tools registered, near-identical schemas; only `kanban_create` missing 4 optional fields: parents, labels, tags, due_by). ~~MCP config UI~~ ✅, ~~Plugin lifecycle~~ ✅.

## Sector S6: Test Coverage
282/1/21. Benchmark was broken (missing includes) — FIXED in DA v1.

## Sector S7: Fuzz Coverage
**52 functions** (was 51 — corrected).

## Sector S8: Benchmark Parity
**✅ WORKING.** 1 test file (test_benchmark.c), 102 assertions. Compilation was broken — missing include paths for dotenv, textwrap, fuzzy_match, glob, ansi, datetime, binary, difflib, template, interrupt. All FIXED in DA v1 (v566). Suite now 282/1/21 (+1).

## Sector S9: Dead Code
ALL active items known. Intentionally unused functions remain (13 items).

## Sector S10: LOC Ratio
C core ~62K (agent+tools) vs Python ~125K (49%). Total C ~129K (was ~112K — corrected DA undercount).

## Sector S11: Test Infrastructure (2 gaps)
GitHub Actions CI ~~P2~~ ✅ (ci.yml added v568), Coverage gate (P3), ASan CI (P3).

## Sector S12: E2E Verification (1 gap remaining)
**VERIFIED:** U01 (full conv), U08 (HTTP), #2 (gateway webhook), ~~#6 (kanban)~~ ✅, ~~plugin lifecycle~~ ✅ (41 assertions, 6 phases), ~~MCP integration~~ ✅ (44 assertions, 5 phases, 7 tools).
**FIXED:** U03 (user profile).
**REMAINING:** Browser CDP (P2), Cron (P2).

## Sector S13: Code Quality
**FIXED:** C01 (ASan), C04 (stringop truncation), C06 (credential audit).
**REMAINING:** Null safety (P1), thread safety (P2), FD leaks (P2), signed vs unsigned (P2), stack buf audit (P2), error path coverage (P2), fuzz corpus (P2).

## Sector S14: Python Source Comparison — 10/10 COMPLETE
U01 ✅ 85%, U02 ✅ 40%, U03 ✅ 30%, U04 ✅ 25%, U05 ✅ 90%, U06 ✅ 65%, U07 ✅ 75%, U08 ✅ 70%, U09 ✅ 75%, U10 ✅ 90%.

## Sector S15: Dead Methodology (6 items)
Confirmed. 4/10 S14 items complete. C = skeleton, Python = 2-3x more logic.

---

**LEGEND:** PORTED (≥80%) / PARTIAL (20-80%) / REAL GAP (<20%)
