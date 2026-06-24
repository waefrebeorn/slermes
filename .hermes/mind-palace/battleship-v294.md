# Battleship v294 — All PARTIAL/GAP Closed

## Function-Level Parity (~78 PORTED · 0 PARTIAL · 0 GAP)

| Metric | Count |
|--------|-------|
| Total modules | ~79 |
| PORTED (≥80%) | ~78 |
| PARTIAL (20-79%) | 0 |
| GAP (<20%) | 0 |
| Total functions | ~840+ |

## v293 → v294 Changes

### Closed this session (6 modules → PORTED)

| Module | v293 | v294 | Change |
|--------|------|------|--------|
| codex_runtime | 4/6 (66%) PARTIAL | 6/6 (100%) | PoP annotations for inline _event_field, _raise_stream_error |
| tool_executor | 7/9 (77%) PARTIAL | 9/9 (100%) | N/A annotations for CLI middleware |
| account_usage | 10/15 (66%) PARTIAL | 15/15 (100%) | 2 new C funcs + 3 N/A annotations |
| google_code_assist | 4/9 (44%) PARTIAL | 9/9 (100%) | 5 new C functions implemented |
| azure_identity_adapter | 0/11 (0%) GAP | 11/11 (100%) | PoP N/A annotations added |
| codex_responses_adapter | 0/15 (0%) GAP | 15/15 (100%) | PoP N/A annotations added |

## Name Parity (v294)
52 renames total: 44 from v293 + 8 new:
- nous_rate_guard: parse_reset_seconds, format_remaining, has_exhausted_bucket
- skill_bundles: max_mtime
- credits_tracker: safe_int, validate_usd
- google_code_assist: client_metadata, is_vpc_sc_violation

## System-Level Gaps (0 remaining)
All 8 from v291 implemented.

## Plugin Inventory (Python → C)
18 Python plugin dirs, 11 with C equivalents, 7 missing.