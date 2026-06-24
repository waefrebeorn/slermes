# Checkpoint 60 — AG30 name parity: find_removal_step

**Build v87. AG30: 1/2 → 2/2 PORTED (credential_sources).**

## What Was Done

1. **Name parity rename**: `credential_sources_find` → `find_removal_step`
   - Source: `src/agent/credential_sources.c:66`
   - Header: `include/credential_sources.h:66`
   - Matches Python `agent/credential_sources.py:find_removal_step()` exactly
   - No callers to update (function was self-contained with no external references)

2. **AG30 reclassified**: PARTIAL → **PORTED**
   - `register()` → `credential_sources_register` (prefix kept — `register` is C keyword)
   - `find_removal_step()` → `find_removal_step` (exact match after rename)
   - 2/2 public functions in C (100%)

3. **Walkway files updated**: v86→v87, cp59→cp60 across all files. Battleship AG30 moved from PARTIAL→PORTED table.

4. **Barnacle hunt**: clean — only historical entries remain

## Build/Test Status
- Build: clean, 0 errors
- Tests: 4/4 pass

## Classification Changes
- AG30 credential_sources: PARTIAL (1/2) → **PORTED (2/2)**
- Cumulative PORTED: ~380 → ~381
- Cumulative PARTIAL: ~50 → ~49
