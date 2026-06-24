/*
 * codex_responses_adapter.c — Port of Python agent/codex_responses_adapter.py
 *
 * Python API → C implementation mapping:
 *   codex_responses_process_message()  → provider_codex_responses.c
 *   codex_responses_parse_response()   → provider_codex_responses.c
 *   codex_responses_build_request()    → provider_codex_responses.c
 *   codex_responses_count_tokens()     → provider_codex_responses.c
 *
 * Codex Responses API provider adapter implemented in provider_codex_responses.c.
 */

#include "provider.h"   /* provider_codex_responses interface via provider dispatch */
