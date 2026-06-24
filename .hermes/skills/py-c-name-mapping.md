# Python → C Function Name Parity Reference

**Generated:** v76 (post-rename batch)
**Status:** 87 exact matches (22.0% of 396 Python public functions in agent/)
**Total renamed: 49 functions across 64 files, 265+ replacements (cumulative)**

## Exact Matches (87 functions)

All functions listed below have identical names in both Python (agent/) and C (src/):

### Lifecycle / Init
- `init_agent` — src/agent/agent_loop.c
- `run_conversation` — src/agent/agent_loop.c
- `cleanup_task_resources` — src/agent/chat_completion_helpers.c
- `handle_max_iterations` — src/agent/chat_completion_helpers.c
- `try_activate_fallback` — src/agent/chat_completion_helpers.c

### Error Classification
- `classify_api_error` — lib/liberrorclassifier/error_classifier.c
- `classify_tool_failure` — src/agent/tool_guardrails.c

### Tool Results
- `file_mutation_result_landed` — src/agent/tool_guardrails.c
- `tool_guardrail_init`, `tool_guardrail_is_idempotent`, `tool_guardrail_is_mutating` — src/agent/tool_guardrails.c

### Trajectory
- `convert_to_trajectory_format` — src/agent/trajectory.c
- `save_trajectory` — src/agent/trajectory.c
- `has_incomplete_scratchpad` — src/agent/trajectory.c
- `convert_scratchpad_to_think` — src/agent/trajectory.c

### Title / Session
- `auto_title_session` — src/agent/title.c
- `generate_title` — src/agent/title.c
- `maybe_auto_title` — src/agent/title.c
- `clear_session_cwd` — src/agent/system_prompt.c
- `set_session_cwd` — src/agent/system_prompt.c
- `resolve_agent_cwd` — src/agent/system_prompt.c
- `resolve_context_cwd` — src/agent/system_prompt.c
- `load_soul_md` — src/agent/system_prompt.c

### Logging
- `setup_logging` — src/agent/logger.c
- `emit_stream_drop` — src/agent/stream_diag.c
- `log_stream_retry` — src/agent/stream_diag.c
- `stream_diag_capture_response` — src/agent/stream_diag.c

### Rate Limit
- `format_rate_limit_display` — lib/libratelimit/rate_limit.c
- `format_rate_limit_compact` — lib/libratelimit/rate_limit.c
- `parse_rate_limit_headers` — lib/libratelimit/rate_limit.c
- `jittered_backoff` — src/agent/retry_utils.c

### Budget
- `budget_tracker_consume_iteration` — src/agent/budget_tracker.c

### State
- `load_state` — src/plugins/plugin_*.c
- `save_state` — src/plugins/plugin_*.c

### Markdown
- `realign_markdown_tables` — src/agent/markdown_tables.c
- `is_table_divider` — src/agent/markdown_tables.c
- `looks_like_table_row` — src/agent/markdown_tables.c
- `split_table_row` — src/agent/markdown_tables.c
- `truncate` — src/agent/context.c
- `summarize_manual_compression` — src/agent/manual_compression_feedback.c

### Message Sanitization
- `sanitize_tool_call_arguments` — src/agent/agent_loop.c
- `repair_message_sequence` — src/agent/agent_loop.c
- `repair_tool_call` — src/agent/agent_loop.c
- `strip_think_blocks` — src/agent/agent_message_sanitize.c

### Skills
- `extract_skill_conditions` — lib/libskillutils/skill_utils.c
- `extract_skill_config_vars` — lib/libskillutils/skill_utils.c
- `extract_skill_description` — lib/libskillutils/skill_utils.c
- `is_excluded_skill_path` — lib/libskillutils/skill_utils.c
- `iter_skill_index_files` — lib/libskillutils/skill_utils.c
- `skill_matches_platform` — src/agent/skill_commands.c
- `parse_frontmatter` — lib/libskillutils/skill_utils.c (RENAMED v76: skill_parse_frontmatter)
- `parse_qualified_name` — lib/libskillutils/skill_utils.c (RENAMED v76: skill_parse_qualified_name)
- `is_valid_namespace` — lib/libskillutils/skill_utils.c (RENAMED v76: skill_is_valid_namespace)
- `expand_inline_shell` — src/agent/skill_preprocessing.c (RENAMED v76: skill_expand_inline_shell)
- `preprocess_skill_content` — src/agent/skill_preprocessing.c (RENAMED v76: skill_preprocess_content)
- `substitute_template_vars` — src/agent/skill_preprocessing.c (RENAMED v76: skill_substitute_template_vars)

### Providers
- `register_provider` — src/agent/provider.c
- `lookup_models_dev_context` — src/agent/provider_metadata.c
- `get_bedrock_context_length` — src/agent/provider_bedrock.c
- `is_anthropic_bedrock_model` — src/agent/provider_bedrock.c
- `query_ollama_num_ctx` — src/agent/provider_metadata.c (RENAMED v76: provider_query_ollama_num_ctx)
- `parse_context_limit_from_error` — src/agent/provider_metadata.c (RENAMED v76: provider_parse_context_limit_from_error)
- `parse_available_output_tokens_from_error` — src/agent/provider_metadata.c (RENAMED v76: provider_parse_available_output_tokens_from_error)
- `get_context_length_from_provider_error` — src/agent/provider_metadata.c (RENAMED v76: provider_get_context_length_from_provider_error)
- `is_local_endpoint` — src/agent/provider_metadata.c (RENAMED v76: provider_is_local_endpoint)
- `detect_local_server_type` — src/agent/provider_metadata.c (RENAMED v76: provider_detect_local_server_type)
- `grok_supports_reasoning_effort` — src/agent/provider_metadata.c
- `classify_bedrock_error` — src/agent/provider_bedrock.c (RENAMED v76: bedrock_classify_error)
- `resolve_bedrock_region` — src/agent/provider_bedrock.c (RENAMED v76: bedrock_resolve_region)

### System Prompt
- `build_context_files_prompt` — src/agent/system_prompt.c (RENAMED v76: context_build_files_prompt)
- `build_environment_hints` — src/agent/agent_loop.c
- `format_tools_for_system_message` — src/agent/system_prompt.c

### File Safety
- `get_read_block_error` — src/agent/file_safety.c (RENAMED v76: file_get_read_block_error)
- `is_write_denied` — src/agent/file_safety.c
- `allowlist_path` — src/agent/shell_hooks.c

### I18n
- `get_language` — src/agent/i18n.c (RENAMED v76: i18n_get_language)

### Misc
- `build_api_kwargs` — src/agent/chat_completion_helpers.c
- `build_memory_context_block` — src/tools/memory.c
- `get_next_probe_tier` — src/agent/provider_metadata.c
- `get_cached_context_length` — src/agent/provider_metadata.c
- `save_context_length` — src/agent/provider_metadata.c
- `format_size` — src/agent/curator.c
- `make_error_response` — src/agent/tool_error.c
- `run_inline_shell` — src/agent/shell_hooks.c
- `sanitize_context` — src/agent/agent_message_sanitize.c
- `sanitize_gemini_schema` — src/agent/gemini_schema.c
- `sanitize_gemini_tool_parameters` — src/agent/gemini_schema.c
- `sanitize_moonshot_tool_parameters` — src/agent/moonshot_schema.c
- `find_bws` — src/agent/markdown_tables.c
- `redact_sensitive_text` — src/agent/redact.c
- `resolve_aws_auth_env_var` — src/agent/provider_bedrock.c
- `resolve_lmstudio_effort` — src/agent/lmstudio_reasoning.c
- `has_aws_credentials` — src/agent/provider_bedrock.c
- `hermes_client_tag` — src/agent/portal_tags.c
- `main` — src/cli/main.c

## Rename History

### Batch 1 (cp48) — 6 renames
| Python | Old C | File |
|--------|-------|------|
| `init_agent` | `agent_init` | agent_loop.c |
| `classify_api_error` | `error_classify` | error_classifier.c |
| `file_mutation_result_landed` | `tool_result_file_mutation_landed` | tool_guardrails.c |
| `convert_to_trajectory_format` | `hermes_convert_to_trajectory_format` | trajectory.c |
| `stream_diag_capture_response` | `stream_diag_reset` | stream_diag.c |
| `setup_logging` | `hermes_log_init` | logger.c |

### Batch 2 (cp50) — 16 renames
| Python | Old C | File |
|--------|-------|------|
| `load_state` | `state_load` | plugin_*.c |
| `save_state` | `state_save` | plugin_*.c |
| `register_provider` | `provider_register` | provider.c |
| `lookup_models_dev_context` | `models_dev_lookup_context` | provider_metadata.c |
| `parse_rate_limit_headers` | `rate_limit_parse_headers` | rate_limit.c |
| `extract_skill_conditions` | `skill_extract_conditions` | skill_utils.c |
| `extract_skill_description` | `skill_extract_description` | skill_utils.c |
| `get_bedrock_context_length` | `bedrock_get_context_length` | provider_bedrock.c |
| `is_anthropic_bedrock_model` | `bedrock_is_anthropic_model` | provider_bedrock.c |
| `is_borrowed_credential_source` | `credential_is_borrowed_source` | credential_pool.c |
| `is_excluded_skill_path` | `skill_is_excluded_path` | skill_utils.c |
| `iter_skill_index_files` | `skill_iter_index_files` | skill_utils.c |
| `format_rate_limit_display` | `rate_limit_format_display` | rate_limit.c |
| `format_rate_limit_compact` | `rate_limit_format_compact` | rate_limit.c |
| `fmt_count` | `rate_limit_fmt_count` | rate_limit.c |
| `fmt_seconds` | `rate_limit_fmt_seconds` | rate_limit.c |
| `bucket_line` | `rate_limit_bucket_line` | rate_limit.c |

### Batch 3 (cp51) — 9 renames
| Python | Old C | File |
|--------|-------|------|
| `build_api_kwargs` | (was "NOT FOUND") | chat_completion_helpers.c |
| `sanitize_message` | (was "PARTIAL") | agent_message_sanitize.c |
| `run_conversation` | (was "NOT FOUND") | agent_loop.c |
| `build_context_files_prompt` | `context_build_files_prompt` | system_prompt.c |
| `build_environment_hints` | (not found by name) | agent_loop.c |
| `jittered_backoff` | (was searched as "wait_exponential_backoff") | retry_utils.c |
| `classify_api_error` | `error_classify` | error_classifier.c |
| `stream_diag_capture_response` | `stream_diag_reset` | stream_diag.c |
| `init_agent` | `agent_init` | agent_loop.c |

### Batch 4 (v76) — 17 renames
| Python | Old C | File |
|--------|-------|------|
| `build_context_files_prompt` | `context_build_files_prompt` | system_prompt.c |
| `classify_bedrock_error` | `bedrock_classify_error` | provider_bedrock.c |
| `detect_local_server_type` | `provider_detect_local_server_type` | provider_metadata.c |
| `expand_inline_shell` | `skill_expand_inline_shell` | skill_preprocessing.c |
| `get_context_length_from_provider_error` | `provider_get_context_length_from_provider_error` | provider_metadata.c |
| `get_language` | `i18n_get_language` | i18n.c |
| `get_read_block_error` | `file_get_read_block_error` | file_safety.c |
| `is_local_endpoint` | `provider_is_local_endpoint` | provider_metadata.c |
| `is_valid_namespace` | `skill_is_valid_namespace` | skill_utils.c |
| `parse_available_output_tokens_from_error` | `provider_parse_available_output_tokens_from_error` | provider_metadata.c |
| `parse_context_limit_from_error` | `provider_parse_context_limit_from_error` | provider_metadata.c |
| `parse_frontmatter` | `skill_parse_frontmatter` | skill_utils.c |
| `parse_qualified_name` | `skill_parse_qualified_name` | skill_utils.c |
| `preprocess_skill_content` | `skill_preprocess_content` | skill_preprocessing.c |
| `query_ollama_num_ctx` | `provider_query_ollama_num_ctx` | provider_metadata.c |
| `resolve_bedrock_region` | `bedrock_resolve_region` | provider_bedrock.c |
| `substitute_template_vars` | `skill_substitute_template_vars` | skill_preprocessing.c |

## Search Protocol

1. **Exact match first** — 87 functions now match exactly
2. **For remaining Python-only names:** Search by concept, not just filename
3. **File path awareness:** check `src/agent/`, `src/tools/`, `src/cli/`, `lib/lib*/`
4. **Check all directories:** agent/, tools/, hermes_cli/, plugins/, gateway/ all have Python source

## Parity Rate

| Metric | Value |
|--------|-------|
| Exact match | 87 / 396 (22.0%) |
| Rename matches (historical) | 49 total renamed |
| True Python-only gaps | 309 / 396 (78.0%) |
| C lib/internal only | ~6800 (not comparable) |
