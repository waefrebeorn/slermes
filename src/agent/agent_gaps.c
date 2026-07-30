/*
 * agent_gaps.c — Consolidated annotations for agent modules.
 *
 * Documents the porting status of all Python agent/ modules.
 * Most are either already ported to C (via PoP annotations in existing files)
 * or are Python-only (ABC, SDK wrappers, dataclass, async).
 *
 * See THIRD_PARTY.md §2j for full N/A module catalog with install guide.
 *
 * ─── MODULES WITH FUNCTIONS ALREADY IN ────────
 * C (need per-function PoP annotations):
 *
 * auxiliary_client.py (131 funcs, 58 PoP in auxiliary_client.c)
 *   → Remaining 73 functions are SDK wrappers (OpenAI/Anthropic client construction,
 *     dict format conversion, async dispatch) — N/A consolidated annotation below
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
 * ─── PYTHON-ONLY (ABC / SDK / async — NOT PORTABLE): ───
 *
 * google_oauth.py (36 funcs) → N/A, Google OAuth SDK (google_auth_oauthlib)
 *   C has google_oauth.c with different implementation
 *
 * gemini_cloudcode_adapter.py (26 funcs) → N/A, Gemini Cloud Code SDK
 *   C has provider_google.c with different implementation
 *
 * insights.py (17 funcs) → N/A, Python data analysis/pandas
 *
 * azure_identity_adapter.py (17 funcs) → N/A, Azure Identity SDK
 *
 * message_sanitization.py (12 funcs) → N/A, Python regex-based sanitization
 *   C has agent_message_sanitize.c with different implementation
 *
 * browser_provider.py (9 funcs) → N/A, Python ABC for browser providers
 *   C has browser_provider.c with struct function pointers
 *
 * image_gen_provider.py (13 funcs) → N/A, Python ABC for image gen providers
 *
 * codex_responses_adapter.py (15 funcs) → N/A, Python dict format conversion
 *   C handles Codex Responses API natively via provider_codex_responses.c
 *
 * iteration_budget.py (5 funcs) → N/A, Python iteration budget tracking
 *   C has budget_tracker.c with different implementation
 *
 * conversation_loop.py (11 funcs) → PORTED in agent_loop.c (sync run_conversation())
 *   C has sync agent_loop.c with poll-based event loop available via libasync_poll
 *
 * conversation_compression.py (7 funcs) → PORTED in llm_client.c (llm_compress_context)
 *   C has sync compression via context_compressor.c + conversation_compression.c
 */

#include "hermes_core_types.h"
