# Battleship v293 — Name Parity + DA Audit

## Function-Level Parity (72 PORTED · 5 PARTIAL · 2 GAP)

| Metric | Count |
|--------|-------|
| Total modules | 79 |
| PORTED (≥80%) | 72 |
| PARTIAL (20-79%) | 5 |
| GAP (<20%) | 2 |
| Total functions | 840/971 (86%) |

### GAP modules
| Module | Status | Functions | Action |
|--------|--------|-----------|--------|
| azure_identity_adapter | GAP (0%) | 0/11 | Azure SDK-specific, N/A candidate |
| codex_responses_adapter | GAP (0%) | 0/15 | Python dict format conversion |

### PARTIAL modules
| Module | Status | Functions | Action |
|--------|--------|-----------|--------|
| account_usage | 66% | 10/15 | 5 missing: _fmt_usd, _is_finite_num, build_nous_credits_snapshot, etc. |
| auxiliary_client | 48% | 52/108 | 56 missing — mostly SDK wrappers |
| codex_runtime | 66% | 4/6 | 2 missing: _event_field, _raise_stream_error |
| google_code_assist | 44% | 4/9 | 5 missing: load_code_assist, onboard_user, etc. |
| tool_executor | 77% | 7/9 | 2 middleware functions |

## System-Level Gaps (0 remaining — all 8 from v291 implemented)

## Name Parity (v293)
44 function renames applied to match Python names 1:1:
- auxiliary_client: 26 functions (stripped auxiliary_ prefix)
- copilot_acp_client: 7 functions (stripped copilot_ prefix)
- google_oauth: 10 functions (stripped google_oauth_ prefix)
- plugin_llm: 1 function (plugin_llm_extract_text → extract_text)

## Plugin Inventory (Python → C)
18 Python plugin dirs, 11 with C equivalents, 7 missing:
- browser, context_engine, dashboard_auth, hermes-achievements, memory, model-providers, platforms, teams_pipeline, video_gen, web
