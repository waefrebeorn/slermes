# Module Map — Python `agent/` ↔ C Implementation

Maps every Python file in `agent/` to its C counterpart.
Files in `src/agent/` marked `(wrapper)` are name-parity wrappers with includes/re-exports; real implementation is in the linked file.

## Agent Core

| Python `agent/` | C Location | Status | Notes |
|---|---|---|---|
| `__init__.py` | — | N/A | Package marker |
| `conversation_loop.py` | `src/agent/conversation_loop.c` | ✅ Ported | run_conversation(), tool dispatch, snapshots |
| `turn_finalizer.py` | `src/agent/turn_finalizer.c` | ✅ Ported | finalize_turn() — post-loop finalization |
| `prompt_builder.py` | `src/agent/prompt_builder.c` | ✅ Ported | Context files, platform hints, environment, skills, CWD |
| `agent_init.py` | `src/agent/agent_init.c` | ✅ Ported | 5/5 functions ported |
| `title_generator.py` | `src/agent/title_generator.c` | ✅ Ported | Thin wrapper over title.c |

## Agent Loop

| Python `agent/` | C Location | Status | Notes |
|---|---|---|---|
| `agent_loop.py` | `src/agent/agent_loop.c` | ✅ Ported | Session management, DB ops, init |
| `agent_runtime_helpers.py` | `src/agent/agent_runtime_helpers.c` | ✅ Ported | `chat_completion_helpers.py` | `src/agent/chat_completion_helpers.c` | ✅ Ported | `iteration_budget.py` | `src/agent/budget_tracker.c` | ✅ Ported | `turn_context.py` | `src/agent/turn_context.c` | ✅ Ported | `turn_retry_state.py` | `src/agent/turn_retry_state.c` | ✅ Ported ## Providers & Adapters

| Python `agent/` | C Location | Status | Notes |
|---|---|---|---|
| `anthropic_adapter.py` | `src/agent/anthropic_adapter.c` | ✅ Wrapper | Impl in provider_anthropic.c |
| `bedrock_adapter.py` | `src/agent/bedrock_adapter.c` | ✅ Wrapper | Impl in provider_bedrock.c |
| `codex_responses_adapter.py` | `src/agent/codex_responses_adapter.c` | ✅ Wrapper | Impl in provider_codex_responses.c |
| `codex_runtime.py` | `src/agent/codex_runtime.c` | ✅ Wrapper | Impl in codex_app_server_session.c |
| `gemini_cloudcode_adapter.py` | `src/agent/gemini_cloudcode_adapter.c` | ✅ Wrapper | Impl in provider_google.c |
| `gemini_native_adapter.py` | `src/agent/gemini_native_adapter.c` | ✅ Wrapper | Impl in provider_google.c |
| `model_metadata.py` | `src/agent/model_metadata.c` | ✅ Wrapper | Impl in provider_metadata.c |
| `models_dev.py` | `src/agent/models_dev.c` | ✅ Wrapper | Impl in provider_metadata.c |
| `provider_metadata.py` | `src/agent/provider_metadata.c` | ✅ Ported ## Compression & Context

| Python `agent/` | C Location | Status | Notes |
|---|---|---|---|
| `context_compressor.py` | `src/agent/context_compressor.c` | ✅ Wrapper | Impl in llm_client.c + context.c |
| `context_engine.py` | `src/agent/context_engine.c` | ✅ Ported | `context_references.py` | `src/agent/context_references.c` | ✅ Ported | `conversation_compression.py` | `src/agent/conversation_compression.c` | ✅ Ported ## Memory & Review

| Python `agent/` | C Location | Status | Notes |
|---|---|---|---|
| `memory_manager.py` | `src/agent/memory_manager.c` | ✅ Wrapper | Impl in hermes_gap_fixes.c + memory_provider.c |
| `memory_provider.py` | `src/agent/memory_provider.c` | ✅ Ported | `background_review.py` | `src/agent/background_review.c` | ✅ Wrapper | Impl in llm_client.c |

## Display & Insights

| Python `agent/` | C Location | Status | Notes |
|---|---|---|---|
| `insights.py` | `src/agent/insights.c` | ✅ Ported | 17/17 functions — full C impl, no pandas |
| `display.py` | `src/cli/display_core.c` | ✅ Ported | CLI display primitives |

## System Prompt

| Python `agent/` | C Location | Status | Notes |
|---|---|---|---|
| `system_prompt.py` | `src/agent/system_prompt.c` | ✅ Ported | Assembly tier |
| `prompt_caching.py` | `src/agent/prompt_caching.c` | ✅ Ported | `runtime_cwd.py` | `src/agent/runtime_cwd.c` | ✅ Wrapper | Impl in prompt_builder.c |

## Security & Sanitization

| Python `agent/` | C Location | Status | Notes |
|---|---|---|---|
| `message_sanitization.py` | `src/agent/message_sanitization.c` | ✅ Wrapper | Impl in sanitize.c + agent_message_sanitize.c |
| `file_safety.py` | `src/agent/file_safety.c` | ✅ Ported | `tool_guardrails.py` | `src/agent/tool_guardrails.c` | ✅ Ported | `url_safety.py` | `src/agent/hermes_url_safety.c` | ✅ Ported ## Tools & Utilities

| Python `agent/` | C Location | Status | Notes |
|---|---|---|---|
| `async_utils.py` | `src/agent/async_utils.c` | ✅ Wrapper | Impl in process_bootstrap.c |
| `skill_utils.py` | `lib/libskillutils/` | ✅ Ported | `tool_dispatch_helpers.py` | `lib/libtooldispatch/` | ✅ Ported | `tool_result_classification.py` | `src/tools/tool_result_classification.c` | ✅ Ported | `error_classifier.py` | `lib/liberrorclassifier/` | ✅ Ported | `rate_limit_tracker.py` | `lib/libratelimit/rate_limit_tracker.c` | ✅ Ported ## Provider-Specific

| Python `agent/` | C Location | Status | Notes |
|---|---|---|---|
| `google_oauth.py` | `src/provider/google_oauth.c` | ✅ Ported | `account_usage.py` | `src/tools/account_usage.c` | ✅ Ported ## Registries

| Python `agent/` | C Location | Status | Notes |
|---|---|---|---|
| `image_gen_provider.py` | `src/tools/image_gen_provider.c` | ✅ Ported | `image_gen_registry.py` | `src/tools/image_gen_registry.c` | ✅ Ported | `tts_provider.py` | `src/tools/tts_provider.c` | ✅ Ported | `tts_registry.py` | `src/tools/tts_registry.c` | ✅ Ported | `video_gen_provider.py` | `src/tools/video_gen_provider.c` | ✅ Ported | `video_gen_registry.py` | `src/tools/video_gen_registry.c` | ✅ Ported | `web_search_registry.py` | `src/tools/web_search_registry.c` | ✅ Ported | `transcription_registry.py` | `lib/libtranscribe/transcription_registry.c` | ✅ Ported | `curator_backup.py` | `src/tools/curator_backup.c` | ✅ Ported | `jiter_preload.py` | `src/jiter_preload.c` | ✅ Ported ## Subdirectory & Hints

| Python `agent/` | C Location | Status | Notes |
|---|---|---|---|
| `subdirectory_hints.py` | `src/agent/subdirectory_hints.c` | ✅ Wrapper | Impl in subdir_hints.c |
| `subdir_hints.py` | `src/agent/subdir_hints.c` | ✅ Ported | `onboarding.py` | `src/agent/onboarding.c` | ✅ Ported | `image_routing.py` | `src/agent/image_routing.c` | ✅ Ported ## Plugin & Hook

| Python `agent/` | C Location | Status | Notes |
|---|---|---|---|
| `plugin_ext.py` | `src/agent/plugin_ext.c` | ✅ Ported | `hook_registry.py` | `src/agent/hook_registry.c` | ✅ Ported ## Totals

| Category | Count | Notes |
|---|---|---|
| Python `agent/` files | 89 | Excluding `__init__.py` |
| Name-parity C wrappers in `src/agent/` | 22 | 1:1 match with Python core agent files |
| C implementations in `src/agent/` | ~65 | Many are single-module files |
| C implementations in `lib/` | ~50 | Sub-libraries for specific features |
| C implementations in `src/tools/` | ~40 | Tool implementations |
| C implementations in `src/provider/` | ~10 | Provider OAuth & token modules |
| C implementations in `src/gateway/` | ~50 | Platform adapters |
| **Total module coverage** | **PORT phase** | Every Python file has a C counterpart or an honest REAL_GAP. Exact counts live in `docs/parity-summary.md` (generated by the live scanner) — never trust a hand-written percentage here. |
