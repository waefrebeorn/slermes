# S14 U06 — Skill System Parity: Python vs C Comparison

## Overview

Phase 502 comparison of the Python skill system (~12,322 LOC, 14 files, ~224 functions) against the C skill system (~7,439 LOC, 12 files, ~204 functions). C/Python ratio: **60.4% — PARTIAL**.

## File-by-File Comparison

| # | Python File | LOC | C File | LOC | C Functions | Verdict | Key Gaps |
|---|-------------|-----|--------|-----|-------------|---------|----------|
| 1 | agent/skill_commands.py | 523 | skill_commands.c | 368 | 11 | PARTIAL ~70% | Platform-aware disabled filtering, payload loading with task_id |
| 2 | agent/skill_preprocessing.py | 139 | skill_preprocessing.c | 304 | 4 | PORTED ~90% | — |
| 3 | agent/skill_utils.py | 566 | skill_utils.c | 651 | 16 | PORTED ~85% | External skills dirs config not ported |
| 4 | agent/skill_bundles.py | 410 | skill_bundles.c | 235 | 8 | PARTIAL ~60% | Bundle composition, filtering, export |
| 5 | hermes_cli/skills_config.py | 177 | — (config.c partial) | — | — | REAL GAP | Platform-specific skill disable/enable, config persistence |
| 6 | hermes_cli/skills_hub.py | 1,708 | skills_hub.c | 245 | 7 | PARTIAL ~50% | CLI browse/install/list UI, search UI |
| 7 | tools/skills_tool.py | 1,524 | skills.c | 2,517 | ~38 | PORTED ~85% | C is more comprehensive, minor filter/search gaps |
| 8 | tools/skill_manager_tool.py | 1,034 | skill_mgmt.c | 1,003 | 22 | PORTED ~90% | Close parity |
| 9 | tools/skill_provenance.py | 78 | skill_provenance.c | 49 | 5 | PORTED ~80% | Minor |
| 10 | tools/skill_usage.py | 608 | skill_usage.c | 571 | 22 | PORTED ~75% | Analytics/recommendation depth |
| 11 | tools/skills_ast_audit.py | 133 | skill_audit.c | 382 | 11 | PORTED ~85% | C is bigger |
| 12 | tools/skills_guard.py | 963 | — | — | — | REAL GAP | No security scanning for external skills |
| 13 | tools/skills_sync.py | 711 | skills_sync.c | 706 | 14 | PORTED ~80% | Checkout/diff/prune depth |
| 14 | tools/skills_hub.py | 3,748 | skills_hub.c | 245 | 7 | PARTIAL ~30% | GitHub auth, source adapters, lock file, quarantine, audit log |

## Summary

- **PORTED (≥80%):** 6 files — skill_preprocessing, skill_utils, skills_tool, skill_manager_tool, skills_ast_audit, skills_sync
- **PARTIAL (20-80%):** 6 files — skill_commands (~70%), skill_bundles (~60%), skills_hub CLI (~50%), skill_usage (~75%), skills_hub hub-tool (~30%), skill_provenance (~80%)
- **REAL GAP (<20%):** 2 files — skills_config.py (177 LOC), skills_guard.py (963 LOC)

**Total LOC:** C ~7,439 vs Python ~12,322 = **60.4% — PARTIAL**

## Implementable Gaps (Next Actions)

### P1: skills_config.py — Platform-specific skill disable/enable
Add `cmd_skills_disable` / `cmd_skills_enable` with platform support to C's commands.c + config.c. ~100-150 LOC.

### P1: skills_guard.py — Security guardrails for external skills
Port the trust-based install policy and regex-based static analysis scanner. ~300-500 LOC combining audit + guard.

### P2: skills_hub.py tool depth — GitHub auth + source adapters
Port GitHubSource, lock file management, quarantine directory, audit log. ~500-800 LOC.

## Priority for Next Phase

The **skill_guard (skills_guard.py)** gap is the most security-critical — without it, installing skills from external hubs has no safety verification. The **skills_config** gap is the smallest (177 LOC) and quickest to implement.
