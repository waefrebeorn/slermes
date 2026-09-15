<!-- PARITY:AUTO -->
| PORTED  | 6,466 / 18,219 (35.5%) |
| REAL_GAP| 11,715 (64.3%) — no N/A |
|  ├ DA-4 stubs | 11 |
|  └ Genuine gaps | 11,704 |
| PARTIAL | 38 (0.2%) |
| BOOTLEG | 0 |
| **Watercup** | 313 PASS / 22,604 (1.4%) · 0/6230 ported match · 11511/11511 gap mismatch |
**Phase (v672):** PORT phase

**Upstream sync:** 2,072 ahead / 2,639 behind (last merge 2026-09-15 (upstream fetched))

_Generated 2026-09-15T06:22:36Z from live scanner + watercup oracle — do not edit by hand._
<!-- /PARITY:AUTO -->

# Water-Cup Gap List — Triple DA Audit

## Audit Date

## Summary

| Metric | Count |
|---|---|
| Total public Python functions (non-dunder, across project) | **8,796** |
| Covered by water-cup gates | **102** (1.16%) |
| **UNCOVERED — gap to close** | **8,694** (98.84%) |
| Water-cup gates existing | 303 (297 function-level + 6 infra/integration) |
| Gates passing (all cups match LIVE Python) | 303/303 (0 failing) |

**VERDICT: 98.84% of public Python functions have no water-cup gate. The project is almost entirely uncupped.**

---

## Existing Water-Cup Coverage (102 Functions)

The following 102 public functions have dedicated water-cup gates that verify C behavior matches LIVE Python:

### agent/ (35 covered, 1463 uncovered)
- `agent/agent_runtime_helpers.py`: repair_empty_non_final_messages, repair_message_sequence, repair_tool_call
- `agent/auxiliary_client.py`: extract_content_or_reasoning  
- `agent/bedrock_adapter.py`: normalize_converse_response
- `agent/billing_usage.py`: usage_model_from_account
- `agent/billing_view.py`: format_money, parse_money
- `agent/context_compressor.py`: name, should_compress_info
- `agent/credits_tracker.py`: is_free_tier_model
- `agent/display.py`: build_tool_preview
- `agent/error_surface.py`: build_error_surface_from_result
- `agent/gemini_native_adapter.py`: is_standard_key_auth_error
- `agent/gemini_schema.py`: sanitize_gemini_schema
- `agent/i18n.py`: t
- `agent/insights.py`: format_terminal
- `agent/kanban_stop.py`: session_called_kanban_terminal
- `agent/message_sanitization.py`: reapply_reasoning_echo
- `agent/moa_loop.py`: peel_reference_guidance
- `agent/model_metadata.py`: is_local_endpoint, parse_available_output_tokens_from_error
- `agent/monitoring/cron_health.py`: classify_cron_error
- `agent/monitoring/gateway_health.py`: classify_gateway_error
- `agent/native_compaction.py`: prune_pre_checkpoint_items
- `agent/prompt_builder.py`: computer_use_guidance
- `agent/prompt_cache_scope.py`: resolve_prompt_cache_scope
- `agent/prompt_caching.py`: apply_anthropic_cache_control
- `agent/reasoning_effort.py`: clamp_effort
- `agent/repetition_guard.py`: is_repetition_dominated
- `agent/retry_utils.py`: parse_retry_after_seconds
- `agent/subscription_view.py`: format_tier_row
- `agent/tool_result_classification.py`: file_mutation_result_landed
- `agent/usage_pricing.py`: format_cost_label, format_token_count_compact

### gateway/ (17 covered, 696 uncovered)
- `gateway/browser_control_broker.py`: browser_control_enabled
- `gateway/channel_directory.py`: resolve_channel_name
- `gateway/display_config.py`: resolve_display_setting
- `gateway/message_timestamps.py`: coerce_message_timestamp
- `gateway/platforms/base.py`: extract_media, resolve_channel_prompt, resolve_channel_skills, safe_url_for_log, truncate_message
- `gateway/platforms/helpers.py`: greedy_pack_blocks, split_at_paragraph_boundary, split_markdown_atoms
- `gateway/platforms/signal_format.py`: markdown_to_signal
- `gateway/platforms/yuanbao.py`: sanitize_markdown_table
- `gateway/platforms/yuanbao_proto.py`: encode_forward_msg_data
- `gateway/response_filters.py`: is_autonomous_silence_response
- `gateway/runtime_footer.py`: resolve_footer_config

### hermes_cli/ (26 covered, 1939 uncovered)
- `hermes_cli/agent_import.py`: extract_markdown_entries
- `hermes_cli/auth.py`: format_auth_error, is_provider_explicitly_configured
- `hermes_cli/commands.py`: get_suggestion
- `hermes_cli/foreign_sessions.py`: parse_codex_session
- `hermes_cli/goals.py`: parse_contract
- `hermes_cli/input_sanitize.py`: collapse_repeated_input_artifacts
- `hermes_cli/loops.py`: format_interval, parse_loop_args
- `hermes_cli/mcp_security.py`: validate_mcp_server_entry
- `hermes_cli/moa_config.py`: normalize_moa_config
- `hermes_cli/model_normalize.py`: detect_vendor, normalize_model_for_provider
- `hermes_cli/models.py`: compute_sale_discount, normalize_opencode_base_url, opencode_model_api_mode
- `hermes_cli/observability/shared_metrics_contract.py`: task_terminal_state, tool_category
- `hermes_cli/plugin_packs.py`: validate_config_seed
- `hermes_cli/plugins.py`: resolve_plugin_load_order
- `hermes_cli/profiles.py`: profiles_to_serve
- `hermes_cli/providers.py`: custom_provider_aliases
- `hermes_cli/session_lost_and_found.py`: classify_lost_and_found_row
- `hermes_cli/sizefmt.py`: format_bytes
- `hermes_cli/voice.py`: normalize_voice_record_key_for_prompt_toolkit
- `hermes_cli/xai_retirement.py`: find_retired_xai_refs

### tools/ (20 covered, 1223 uncovered)
- `tools/ansi_strip.py`: strip_ansi
- `tools/audio_container.py`: sniff_container
- `tools/binary_extensions.py`: has_binary_extension
- `tools/clarify_tool.py`: clarify_tool, mark_recommended, strip_recommended
- `tools/delegation_output_schema.py`: extract_json_candidate
- `tools/env_passthrough.py`: register_env_passthrough
- `tools/fuzzy_match.py`: find_closest_lines, fuzzy_find_and_replace
- `tools/mcp_tool.py`: sanitize_mcp_name_component
- `tools/memory_tool.py`: apply_batch
- `tools/patch_parser.py`: parse_v4a_patch
- `tools/path_security.py`: has_traversal_component
- `tools/schema_sanitizer.py`: strip_pattern_and_format, strip_slash_enum
- `tools/threat_patterns.py`: scan_for_threats
- `tools/tour_tool.py`: tour_tool
- `tools/url_safety.py`: normalize_url_for_request, sensitive_query_param_name

### Other modules (4 covered)
- `cron/jobs.py`: normalize_workdir (1 covered, 127 uncovered)
- `hermes_constants.py`: resolve_reasoning_config (1 covered, 44 uncovered)
- `hermes_state.py`: classify_persistence_error (1 covered, 185 uncovered)
- `toolsets.py`: resolve_toolset (1 covered, 8 uncovered)

---

## The Gap: 8,694 Uncovered Public Functions

### By module (top 20 by magnitude)

| Module | Covered | Uncovered | Total | Coverage % |
|---|---|---|---|---|
| hermes_cli | 26 | 1939 | 1965 | 1.3% |
| agent | 35 | 1463 | 1498 | 2.3% |
| tools | 20 | 1223 | 1243 | 1.6% |
| tests | 0 | 1134 | 1134 | 0.0% |
| gateway | 17 | 696 | 713 | 2.4% |
| plugins | 0 | 640 | 640 | 0.0% |
| scripts | 0 | 311 | 311 | 0.0% |
| skills | 0 | 249 | 249 | 0.0% |
| optional-skills | 0 | 194 | 194 | 0.0% |
| hermes_state | 1 | 185 | 186 | 0.5% |
| cron | 1 | 127 | 128 | 0.8% |
| tui_gateway | 0 | 120 | 120 | 0.0% |
| cli | 0 | 109 | 109 | 0.0% |
| hermes_constants | 1 | 44 | 45 | 2.2% |
| evals | 0 | 39 | 39 | 0.0% |
| website | 0 | 33 | 33 | 0.0% |
| acp_adapter | 0 | 22 | 22 | 0.0% |
| run_agent | 0 | 21 | 21 | 0.0% |
| utils | 0 | 20 | 20 | 0.0% |
| mcp_serve | 0 | 18 | 18 | 0.0% |
| (remaining 25 modules) | 2 | 346 | 348 | — |
| **TOTAL** | **102** | **8,694** | **8,796** | **1.16%** |

### Honest assessment

**This is NOT a small gap.** The project has 8,796 public functions and only 102 have water-cup gates. The existing 303 gates cover <2% of the public API surface.

The gates that DO exist are high-quality — all 303 pass, every cup matches LIVE Python. But they are a drop in the ocean.

---

## Gate Inventory (303 total)

### Function-level gates (297)
Each maps to one specific Python function via `py_module` + `py_fn` in gate.json.

### Integration/property/infra gates (6)
Gates that test behavior without mapping 1:1 to a single Python function:
- `api_error_summary_ladder` — error summary ladder (consumes `run_agent._summarize_api_error`)
- `approval_command_matches_permanent_allowlist` — policy gate (consumer of `tools.approval._command_matches_permanent_allowlist`)
- `crowd_chat_protocol_basic_connectivity` — protocol-level integration
- `tool_idempotency` — contract-based gate (no LIVE Python twin; self-contained parity)
- `tool_result_classification` — contract-based gate (no LIVE Python twin)
- `web_auth` — tests `PUBLIC_API_PATHS` constant from `hermes_cli/dashboard_auth/public_paths.py`

---

## Priorities for Closing the Gap

### Immediate wins (already in C binary, just need gates)
Many functions are already implemented in C (`.c` files in slermes/src/) but lack water-cup gate directories. These are the fastest wins — the C code exists, just create the twin/harness/gate.json.

### Next tier (portable Python → C)
Pure logic functions that are tractable to port to C and then cup.

### Lower priority
- Test files (1,134 functions in tests/) — these are test utilities, not production code
- Plugin/skill infrastructure — large surface area, lower criticality

---

## Methodology

This audit uses triple-Devil's-Advocate verification:
1. **DA 1**: Count every public non-dunder Python function across the entire project using AST parsing
2. **DA 2**: Match against every existing water-cup gate's `py_module` + `py_fn` field
3. **DA 3**: Verify gate.json `name` field matches directory name (12 mismatches found and noted)

Coverage = (functions with a matching gate) / (total public non-dunder functions).

Functions are "covered" only if a gate exists with the exact `py_module` + `py_fn` pair. Partial coverage (gate exists for a related function in the same module) does NOT count.

---

## Action Items

1. **Wire gates for already-ported C functions** — fastest path to more cups
2. **Port + cup remaining REAL_GAP modules** — follow existing close-gaps methodology
3. **Triage the 8,694 gap** by module priority — what matters most to cup first?
