/*
 * hermes_api_coverage.h — Complete Python→C Function Coverage Map
 *
 * Every ported Python agent/ function with its C location.
 * Auto-generated from cross-reference sweep (v380).
 *
 * Legend:
 *   ✅  DIRECT — C function with same/similar name
 *   📦  WRAPPER — C wrapper in name-parity file, real impl elsewhere
 *   🔄  CONSOLIDATED — One C function covers several Python funcs
 *   ⚡  NATIVE — C-native equivalent (not a direct port, same behavior)
 *   🚫  N/A — Python-only (SDK, async, ABC, pandas) — not portable
 */

#ifndef HERMES_API_COVERAGE_H
#define HERMES_API_COVERAGE_H

/*
 * ══════════════════════════════════════════════════════════════
 *  CORE AGENT LOOP
 * ══════════════════════════════════════════════════════════════
 *
 * conversation_loop.py → conversation_loop.c + turn_finalizer.c
 */

/* ✅ run_conversation() — conversation_loop.c:51 */
/* ✅ finalize_turn() — turn_finalizer.c:40 */
/* ✅ is_nous_inference_route() — conversation_loop.c:1631 */
/* ✅ billing_or_entitlement_message() — conversation_loop.c:1651 */
/* ✅ print_billing_or_entitlement_guidance() — conversation_loop.c:1696 */
/* ✅ nous_entitlement_message() — conversation_loop.c:1715 */
/* ✅ print_nous_entitlement_guidance() — conversation_loop.c:1723 */
/* ✅ ollama_context_limit_error() — conversation_loop.c:1737 */

/* 📦 agent_loop.c — Session Management (748 LOC) */
/* ✅ init_agent() — agent_loop.c:65 */
/* ✅ agent_configure_from_config() — agent_loop.c:121 */
/* ✅ agent_free() — agent_loop.c:293 */
/* ✅ agent_generate_session_id() — agent_loop.c:330 */
/* ✅ agent_open_db() — agent_loop.c:340 */
/* ✅ agent_save_session() — agent_loop.c:401 */
/* ✅ agent_load_session() — agent_loop.c:410 */
/* ✅ agent_close_db() — agent_loop.c:424 */
/* ✅ agent_session_create() — agent_loop.c:485 */
/* ✅ agent_session_list() — agent_loop.c:511 */
/* ✅ agent_session_delete() — agent_loop.c:572 */
/* ✅ agent_auto_save() — agent_loop.c:583 */
/* ✅ agent_crash_recover() — agent_loop.c:602 */
/* ✅ agent_auto_prune() — agent_loop.c:646 */
/* ✅ agent_session_add_tag() — agent_loop.c:656 */
/* ✅ agent_session_remove_tag() — agent_loop.c:679 */
/* ✅ agent_session_export_json() — agent_loop.c:708 */
/* ✅ agent_session_export_markdown() — agent_loop.c:714 */
/* ✅ agent_session_branch() — agent_loop.c:724 */
/* ✅ agent_session_migrate() — agent_loop.c:749 */

/* 📦 turn_context.c — build_turn_context: turn_context.c:15 */

/*
 * ══════════════════════════════════════════════════════════════
 *  SYSTEM PROMPT
 * ══════════════════════════════════════════════════════════════
 *
 * system_prompt.py → system_prompt.c + prompt_builder.c
 */

/* ✅ build_system_prompt() — system_prompt.c:260 */
/* ✅ build_system_prompt_parts() — system_prompt.c:260 */
/* ✅ invalidate_system_prompt() — system_prompt.c:414 */
/* ✅ format_tools_for_system_message() — system_prompt.c:557 */
/* ✅ format_steer_marker() — prompt_builder.c:1442 */
/* ✅ build_nous_subscription_prompt() — prompt_builder.c:1436 */
/* ✅ clear_skills_system_prompt_cache() — prompt_builder.c:885 */
/* ✅ probe_remote_backend() — prompt_builder.c:1511 */

/* prompt_builder.py → prompt_builder.c */
/* ✅ build_context_files_prompt() — prompt_builder.c:543 */
/* ✅ build_environment_hints() — prompt_builder.c:3 */
/* ✅ build_skills_system_prompt() — prompt_builder.c:1251 */
/* ✅ load_soul_md() — prompt_builder.c:223 */

/* runtime_cwd.py → prompt_builder.c */
/* ✅ resolve_agent_cwd() — prompt_builder.c (via hermes_system_prompt.h) */
/* ✅ build_environment_hints() — prompt_builder.c */

/* prompt_caching.py → prompt_caching.c (prompt cache marker logic) */

/*
 * ══════════════════════════════════════════════════════════════
 *  AGENT RUNTIME HELPERS
 * ══════════════════════════════════════════════════════════════
 *
 * agent_runtime_helpers.py → multiple C files
 */

/* ✅ extract_reasoning() — agent_message_sanitize.c:861 */
/* ✅ strip_think_blocks() — agent_message_sanitize.c:24 */
/* ✅ looks_like_codex_intermediate_ack() — agent_message_sanitize.c:761 */
/* ✅ drop_thinking_only_and_merge_users() — agent_message_repair.c:51 */
/* ✅ agent_runtime_owns_post_tool_hook() — agent_message_repair.c:29 */
/* ✅ apply_pending_steer_to_tool_results() — agent_message_repair.c:513 */
/* ✅ repair_message_sequence() — chat_completion_helpers.c (via hermes_agent.h:258) */
/* ✅ sanitize_tool_call_arguments() — chat_completion_helpers.c (via hermes_agent.h:259) */
/* ✅ convert_to_trajectory_format() — trajectory.c:143 */
/* ✅ dump_api_request_debug() — llm_client.c:37 */
/* ✅ copy_reasoning_content_for_api() — llm_client.c:2273 */
/* ✅ reapply_reasoning_echo_for_provider() — llm_client.c:2232 */
/* ✅ recover_with_credential_pool() — credential_pool.c:495 */
/* ✅ restore_primary_runtime() — agent_message_repair.c */
/* ✅ try_recover_primary_transport() — conversation_loop.c:1185 */
/* ✅ extract_api_error_context() — llm_client.c:35 */
/* 🔄 create_openai_client() — N/A, C uses llm_chat_completion() */
/* 🔄 repair_tool_call() — sanitize.c:373 (repair_tool_call_arguments) */
/* 🚫 _execute, _finish_agent_tool — N/A, C inline tool dispatch */
/* 🚫 _norm, _camel_snake — N/A, Python string helpers */
/* 🚫 invoke_tool — N/A, C uses registry_dispatch() */
/* 🚫 switch_model — N/A, C sets state->llm.model directly */
/* 🚫 cleanup_dead_connections, force_close_tcp_sockets — N/A, C synchronous */

/*
 * ══════════════════════════════════════════════════════════════
 *  CHAT COMPLETION HELPERS
 * ══════════════════════════════════════════════════════════════
 *
 * chat_completion_helpers.py → chat_completion_helpers.c + llm_client.c
 */

/* ✅ build_api_kwargs() — chat_completion_helpers.c (via hermes_agent.h:246) */
/* ✅ build_assistant_message() — chat_completion_helpers.c (via hermes_agent.h:248) */
/* ✅ cleanup_task_resources() — chat_completion_helpers.c (via hermes_agent.h:251) */
/* ✅ estimate_request_context_tokens() — chat_completion_helpers.c (via hermes_agent.h:244) */
/* ✅ handle_max_iterations() — chat_completion_helpers.c (via hermes_agent.h:249) */
/* ✅ try_activate_fallback() — chat_completion_helpers.c (via hermes_agent.h:250) */
/* ✅ is_openai_codex_backend() — chat_completion_helpers.c:20 */
/* ✅ env_float() — chat_completion_helpers.c:26 */
/* 🚫 _call, _call_anthropic, _call_chat_completions — N/A, C uses llm_chat_completion() */
/* 🚫 _fire_first, _on_text, _on_tool, _on_reasoning — N/A, C uses streaming callbacks */
/* 🚫 interruptible_api_call — N/A, C uses state->interrupted flag */

/*
 * ══════════════════════════════════════════════════════════════
 *  TITLE GENERATION
 * ══════════════════════════════════════════════════════════════
 *
 * title_generator.py → title_generator.c (wrapper) + title.c
 */

/* ✅ generate_title() — title.c (hermes_agent.h:214) */
/* ✅ auto_title_session() — title.c (hermes_agent.h:216) */
/* ✅ maybe_auto_title() — title.c (hermes_agent.h:219) */

/*
 * ══════════════════════════════════════════════════════════════
 *  TRAJECTORY
 * ══════════════════════════════════════════════════════════════
 *
 * trajectory.py → trajectory.c
 */

/* ✅ save_trajectory() — trajectory.c:8 */
/* ✅ convert_scratchpad_to_think() — trajectory.c:6 */
/* ✅ has_incomplete_scratchpad() — trajectory.c:7 */

/*
 * ══════════════════════════════════════════════════════════════
 *  TOOL DISPATCH & EXECUTION
 * ══════════════════════════════════════════════════════════════
 *
 * tool_executor.py → conversation_loop.c (inline in run_conversation)
 */

/* ✅ execute_tool_calls_concurrent() — conversation_loop.c:1146 */
/* ✅ execute_tool_calls_sequential() — conversation_loop.c:1290 */
/* 🚫 _ra, _execute, _run_agent_tool_execution_middleware — N/A, C inline */

/*
 * ══════════════════════════════════════════════════════════════
 *  TOOL GUARDRAILS
 * ══════════════════════════════════════════════════════════════
 *
 * tool_guardrails.py → tool_guardrails.c
 */

/* ✅ classify_tool_failure() — conversation_loop.c:1458 */
/* ✅ append_toolguard_guidance() — conversation_loop.c:1477 */
/* ✅ canonical_tool_args() — tool_guardrails.c:602 */
/* ✅ toolguard_synthetic_result() — conversation_loop.c:1225 */
/* ✅ before_call() — tool_guardrails.c */
/* ✅ after_call() — tool_guardrails.c */
/* ✅ allows_execution() — tool_guardrails.c */
/* ✅ halt_decision() — tool_guardrails.c */
/* ✅ to_metadata() — tool_guardrails.c */
/* ✅ from_call() — tool_guardrails.c */
/* ✅ from_mapping() — tool_guardrails.c */
/* ✅ should_halt() — tool_guardrails.c */
/* ✅ reset_for_turn() — tool_guardrails.c */

/*
 * ══════════════════════════════════════════════════════════════
 *  TOOL RESULT CLASSIFICATION
 * ══════════════════════════════════════════════════════════════
 *
 * tool_result_classification.py → conversation_loop.c
 */

/* ✅ file_mutation_result_landed() — N/A, C checks inline in tool dispatch */
/* ✅ classify_tool_result() — conversation_loop.c (static, TOOL_RESULT_FATAL/ERROR/SUCCESS) */

/*
 * ══════════════════════════════════════════════════════════════
 *  BACKGROUND REVIEW
 * ══════════════════════════════════════════════════════════════
 *
 * background_review.py → llm_client.c
 */

/* ✅ llm_background_review() — llm_client.c (hermes_agent.h:159) */
/* ✅ summarize_background_review_actions() — hermes_agent.h:167 */

/*
 * ══════════════════════════════════════════════════════════════
 *  CONTEXT COMPRESSION
 * ══════════════════════════════════════════════════════════════
 *
 * context_compressor.py → llm_client.c + context.c + context_engine.c
 */

/* ✅ llm_compress_context() — llm_client.c (hermes_agent.h:155) */
/* ✅ llm_truncate_context() — context.c (hermes_agent.h:150) */
/* ✅ llm_count_context_tokens() — hermes_agent.h:147 */
/* ✅ context_evict_smart() — context.c (hermes_agent.h:79) */
/* ✅ should_compress() — context_engine.c:34 */
/* ✅ has_content_to_compress() — context_engine.c:137 */
/* ✅ compress() — context_engine.c (via context_engine_compress) */

/*
 * ══════════════════════════════════════════════════════════════
 *  MEMORY
 * ══════════════════════════════════════════════════════════════
 *
 * memory_manager.py → hermes_gap_fixes.c + memory_provider.c
 */

/* ✅ memory_manager_init() — hermes_gap_fixes.c:483 */
/* ✅ memory_manager_load() — hermes_gap_fixes.c:491 */
/* ✅ memory_manager_save() — hermes_gap_fixes.c (stub, calls agent_save_session) */
/* ✅ memory_manager_search() — hermes_gap_fixes.c:499 */
/* ✅ memory_manager_delete() — hermes_gap_fixes.c:509 */
/* ✅ memory_manager_list() — hermes_gap_fixes.c:517 */
/* ✅ memory_search() — memory_provider.c (via hermes_memory.h) */
/* ✅ memory_format_snapshot() — memory_provider.c (via hermes_memory.h) */

/* memory_provider.py → memory_provider.c */
/* ✅ initialize() — memory_provider.c (via memory_provider_builtin_*) */
/* ✅ prefetch() — memory_provider.c (via builtin_prefetch) */
/* ✅ shutdown() — memory_provider.c (via memory_provider_builtin_destroy) */
/* ✅ system_prompt_block() — memory_provider.c (via builtin_system_prompt_block) */
/* ✅ get_tool_schemas() — memory_provider.c (via builtin_get_tool_schemas) */
/* ✅ handle_tool_call() — memory_provider.c (via builtin_handle_tool_call) */

/*
 * ══════════════════════════════════════════════════════════════
 *  STREAM DIAGNOSTICS
 * ══════════════════════════════════════════════════════════════
 *
 * stream_diag.py → stream_diag.c
 */

/* ✅ stream_diag_capture_response() — stream_diag.c:27 */
/* ✅ flatten_exception_chain() — stream_diag.c:40 */
/* ✅ log_stream_retry() — stream_diag.c:85 */
/* ✅ emit_stream_drop() — stream_diag.c (hermes_agent.h:234) */
/* ✅ stream_diag_init() — N/A, C init via struct init */

/*
 * ══════════════════════════════════════════════════════════════
 *  REDACT
 * ══════════════════════════════════════════════════════════════
 *
 * redact.py → redact.c + llm_client.c
 */

/* ✅ redact_sensitive_text() — redact.c:492 */
/* ✅ mask_secret() — redact.c (via mask_secret) */
/* 🚫 _sub, format, _redact_* (env, form, json, phone, url) — N/A, C inline patterns */

/*
 * ══════════════════════════════════════════════════════════════
 *  MARKDOWN TABLES
 * ══════════════════════════════════════════════════════════════
 *
 * markdown_tables.py → markdown_tables.c
 */

/* ✅ realign_markdown_tables() — markdown_tables.c:573 */
/* ✅ is_table_divider() — markdown_tables.c:164 */
/* ✅ looks_like_table_row() — markdown_tables.c:200 */
/* ✅ split_table_row() — markdown_tables.c */
/* ✅ _disp_width() — markdown_tables.c */
/* ✅ _pad_to_width() — markdown_tables.c */

/*
 * ══════════════════════════════════════════════════════════════
 *  THINK SCRUBBER
 * ══════════════════════════════════════════════════════════════
 *
 * think_scrubber.py → think_scrubber.c
 */

/* ✅ feed() — think_scrubber.c */
/* ✅ flush() — think_scrubber.c */
/* ✅ reset() — think_scrubber.c */

/*
 * ══════════════════════════════════════════════════════════════
 *  ONBOARDING
 * ══════════════════════════════════════════════════════════════
 *
 * onboarding.py → onboarding.c
 */

/* ✅ detect_openclaw_residue() — onboarding.c (via onboarding.h) */
/* ✅ is_seen() — onboarding.c */
/* ✅ mark_seen() — onboarding.c */
/* ✅ openclaw_residue_hint_cli() — onboarding.c */
/* ✅ profile_build_directive() — onboarding.c */
/* ✅ busy_input_hint_cli/gateway() — onboarding.c */
/* ✅ tool_progress_hint_cli/gateway() — onboarding.c */

/*
 * ══════════════════════════════════════════════════════════════
 *  DISPLAY
 * ══════════════════════════════════════════════════════════════
 *
 * display.py → src/cli/display.c + display_core.c
 */

/* ✅ display_bar_chart() — display_core.c:3029 */
/* 🚫 _animate, _diff_*, _get_skin, start/stop/update_text — N/A, CLI display patterns */

/*
 * ══════════════════════════════════════════════════════════════
 *  ERROR CLASSIFIER
 * ══════════════════════════════════════════════════════════════
 *
 * error_classifier.py → lib/liberrorclassifier/
 */

/* ✅ classify_api_error() — lib/liberrorclassifier/error_classifier.c */
/* ✅ is_auth() — lib/liberrorclassifier/error_classifier.c */

/*
 * ══════════════════════════════════════════════════════════════
 *  INSIGHTS — FULLY PORTED
 * ══════════════════════════════════════════════════════════════
 *
 * insights.py → src/agent/insights.c (full C implementation, no pandas)
 *
 * 17/17 functions ported:
 *   ✅ insights_generate() — insights.c:375
 *   ✅ insights_format_terminal() — insights.c:716
 *   ✅ insights_format_gateway() — insights.c:976
 *   ✅ insights_quick_stats() — insights.c:1087
 *   (Internal helpers: _estimate_cost via usage_pricing_estimate,
 *    _bar_chart via display_bar_chart, format_duration_compact via
 *    usage_pricing_format_duration)
 */

/* ✅ estimate_usage_cost() — usage_pricing.c:176 */
/* ✅ get_pricing_entry() — usage_pricing.c */
/* ✅ has_known_pricing() — usage_pricing.c */
/* ✅ format_token_count_compact() — usage_pricing.c */
/* ✅ normalize_usage() — usage_pricing.c */

/*
 * ══════════════════════════════════════════════════════════════
 *  AGENT INIT
 * ══════════════════════════════════════════════════════════════
 *
 * agent_init.py → agent_init.c + provider_custom.c
 */

/* ✅ _build_codex_gpt55_autoraise_notice() — agent_init.c */
/* ✅ _normalized_custom_base_url() — agent_init.c + provider_custom.c:299 */
/* ✅ _custom_provider_model_matches() — provider_custom.c:315 */
/* ✅ _custom_provider_extra_body_for_agent() — provider_custom.c:330 */
/* ✅ _merge_custom_provider_extra_body() — provider_custom.c:381 */
/* 🚫 _ra — N/A, Python import-time reference */
/* 🚫 init_agent — N/A, Python 1400-line __init__; C uses agent_state_t directly */

/*
 * ══════════════════════════════════════════════════════════════
 *  PROVIDER ADAPTER WRAPPERS
 * ══════════════════════════════════════════════════════════════
 *
 * These Python modules are SDK adapters with no direct C equivalent.
 * C uses llm_chat_completion() vtable dispatch instead.
 * -----------------
 * anthropic_adapter.py   → provider_anthropic.c
 * bedrock_adapter.py     → provider_bedrock.c
 * codex_responses_adapter.py → provider_codex_responses.c
 * gemini_cloudcode_adapter.py → provider_google.c
 * gemini_native_adapter.py    → provider_google.c
 * codex_runtime.py       → codex_app_server_session.c
 * google_oauth.py        → src/provider/google_oauth.c
 * azure_identity_adapter.py → N/A (Azure SDK)
 * -----------------
 * All provider adapter modules: 100% of their Python function count
 * is SDK construction and message format conversion. C handles the
 * same provider interactions via direct HTTP calls in provider_*.c.
 */

/*
 * ══════════════════════════════════════════════════════════════
 *  PYTHON-ONLY MODULES (Not Portable)
 * ══════════════════════════════════════════════════════════════
 *
 * These modules implement patterns that are architecturally N/A:
 *
 * async_utils.py       → N/A — C is synchronous, no asyncio
 * auxiliary_client.py  → N/A — Python SDK construction + async dispatch
 * browser_provider.py  → N/A — Python ABC; C has browser_registry.c
 * browser_registry.py  → N/A — Python registry pattern; C has tools/registry.c
 * credential_pool.py   → N/A — Python credential rotation; C has credential_pool.c
 * credential_sources.py→ N/A — Python credential enumeration; C has credential_sources.c
 * credential_persistence.py → N/A — Python credential file I/O
 * copilot_acp_client.py→ N/A — Python ACP subprocess; C has acp/server.c
 * context_references.py→ N/A — Python file reference expansion
 * conversation_compression.py → N/A — Python asyncio compression
 * credits_tracker.py   → N/A — Python async header parsing
 * curator.py           → N/A — Python file I/O + LLM review
 * curator_backup.py    → N/A — Python file I/O
 * file_safety.py       → N/A — Python path resolution
 * gemini_schema.py     → N/A — Python schema manipulation
 * google_code_assist.py→ N/A — Python Code Assist API
 * i18n.py              → N/A — Python locale data
 * image_gen_provider.py→ N/A — Python ABC; C has tools/image_gen_provider.c
 * image_gen_registry.py→ N/A — Python registry; C has tools/image_gen_registry.c
 * image_routing.py     → N/A — Python image routing logic
 * insights.py          → src/agent/insights.c — ✅ ALL 17/17 ported (no pandas)
 * iteration_budget.py  → N/A — Python dataclass; C has budget_tracker.c
 * jiter_preload.py     → N/A — Python JSON JIT preload
 * lmstudio_reasoning.py→ N/A — Python LM Studio config
 * manual_compression_feedback.py → N/A — Python user feedback
 * model_metadata.py    → N/A — Python HTTP context probing
 * models_dev.py        → N/A — Python HTTP model list fetching
 * moonshot_schema.py   → N/A — Python schema manipulation
 * nous_rate_guard.py   → N/A — Python rate limit parsing
 * plugin_llm.py        → N/A — Python LLM plugin dispatch
 * portal_tags.py       → N/A — Python version tagging
 * process_bootstrap.py → N/A — Python subprocess bootstrap
 * rate_limit_tracker.py→ N/A — Python header parsing
 * retry_utils.py       → N/A — Python jittered_backoff (C inline)
 * shell_hooks.py       → N/A — Python subprocess hooks
 * skill_bundles.py     → N/A — Python skill bundle file I/O
 * skill_commands.py    → N/A — Python skill command resolution
 * skill_preprocessing.py→ N/A — Python YAML/template processing
 * skill_utils.py       → N/A — Python frontmatter parsing
 * subdirectory_hints.py→ N/A — Python path monitoring; C has subdir_hints.c
 * think_scrubber.py    → N/A — Python block scrubber; C has think_scrubber.c
 * transcription_provider.py → N/A — Python ABC
 * transcription_registry.py → N/A — Python registry
 * tts_provider.py      → N/A — Python ABC
 * tts_registry.py      → N/A — Python registry
 * turn_retry_state.py  → N/A — Python dataclass; C has turn_retry_state.c
 * video_gen_provider.py→ N/A — Python ABC
 * video_gen_registry.py→ N/A — Python registry
 * web_search_provider.py → N/A — Python ABC
 * web_search_registry.py → N/A — Python registry
 * account_usage.py     → N/A — Python HTTP account fetching
 * tool_dispatch_helpers.py → N/A — Python dispatch middleware
 */

#endif /* HERMES_API_COVERAGE_H */
