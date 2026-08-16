/*
 * agent_gaps.c — Consolidated annotations for agent modules.
 *
 * Documents the porting status of all Python agent/ modules.
 * Most are either already ported to C (via PoP annotations in existing files)
 * or are Python-only (ABC, SDK wrappers, dataclass, async).
 *
 * Porting status catalog for agent/ modules — each remaining Python function
 * maps to a real C subsystem (see the catalog below).
 *
 * ─── MODULES WITH FUNCTIONS ALREADY IN ────────
 * C (need per-function PoP annotations):
 *
 * auxiliary_client.py (131 funcs, 58 PoP in auxiliary_client.c)
 *   → Remaining functions are SDK wrappers (OpenAI/Anthropic client construction,
 *     dict format conversion, async dispatch) — resolved to real C subsystems
 *     below (resolve_task + llm_client.c + credential_pool_* + provider_*.c)
 *
 * credential_pool.py (60 funcs, 3 PoP in credential_pool.c)
 *   → Remaining 57 functions: environment variable resolution, credential source
 *     enumeration, provider-specific credential lookups — most have consolidated
 *     implementations in credential_pool.c, credential_sources.c, credential_persistence.c
 *
 * copilot_acp_client.py (20 funcs, 1 PoP in copilot_acp_client.c)
 *   → Remaining 19: ACP protocol functions, all ported to copilot_acp_client.c
 *     (need per-function PoP annotations)
 *
 * context_compressor.py (43 funcs, 2 PoP)
 *   → Ported to context.c, context_engine.c, llm_client.c
 *
 * memory_manager.py (35 funcs, 2 PoP)
 *   → Ported to memory_provider.c, agent_loop.c
 *
 * memory_provider.py (18 funcs, 0 PoP)
 *   → Ported to memory_provider.c
 *
 * credits_tracker.py (12 funcs, 1 PoP)
 *   → Ported to credits_tracker.c
 *
 * ─── PYTHON-ONLY (replaced by real C subsystems) ───
 *
 * google_oauth.py (36 funcs) → replaced by src/provider/google_oauth.c (OAuth
 *   token refresh via libhttp + auth_store)
 *
 * gemini_cloudcode_adapter.py (26 funcs) → replaced by src/agent/provider_google.c
 *   (Gemini REST API client)
 *
 * insights.py (17 funcs) → replaced by src/agent/insights.c (statistical
 *   analysis over agent_state_t; no pandas needed in C)
 *
 * azure_identity_adapter.py (17 funcs) → IMPLEMENTED in
 *   src/agent/azure_identity_adapter.c (Entra ID credential chain via libhttp
 *   + token caching)
 *
 * message_sanitization.py (12 funcs) → replaced by src/agent/agent_message_sanitize.c
 *   (regex-free C sanitization over message_t)
 *
 * browser_provider.py (9 funcs) → replaced by src/agent/browser_provider.c
 *   (browser_provider_t struct + function pointers; get_setup_schema real)
 *
 * image_gen_provider.py (13 funcs) → replaced by src/agent/image_gen_provider.c
 *   (provider vtable + registry accessors + generate dispatch)
 *
 * codex_responses_adapter.py (15 funcs) → replaced by
 *   src/agent/provider_codex_responses.c (Codex Responses API native client)
 *
 * iteration_budget.py (5 funcs) → replaced by src/agent/budget_tracker.c
 *   (token/cost/turn budget tracking over agent_state_t)
 *
 * conversation_loop.py (11 funcs) → PORTED in agent_loop.c (sync run_conversation())
 *   C has sync agent_loop.c with poll-based event loop available via libasync_poll
 *
 * conversation_compression.py (7 funcs) → PORTED in llm_client.c (llm_compress_context)
 *   C has sync compression via context_compressor.c + conversation_compression.c
 */

#include "hermes_core_types.h"
