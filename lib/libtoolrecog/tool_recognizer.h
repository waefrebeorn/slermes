/*
 * tool_recognizer.h — Centralized tool-call recognition/normalization.
 *
 * Faithful port of the front half of Hermes's tool-call recognition
 * pipeline (agent/tool_executor.py + agent/chat_completion_helpers.py):
 *
 *   Stage 2  Deterministic call-id generation — if the model omits the
 *            tool_call id, synthesize a stable one (Hermes
 *            _deterministic_call_id).
 *   Stage 4  Malformed-arguments handling — if `arguments` is not a valid
 *            JSON object, mark the call malformed; the dispatch loop emits
 *            the "Invalid tool arguments, tool was not executed" result
 *            WITHOUT invoking the handler (Hermes _parse_tool_arguments).
 *   Stage 5  Tool-Search unwrap — if the model invoked the `tool_call`
 *            bridge, peel it open to the underlying tool and enforce
 *            session scope before dispatch (Hermes tool_search.resolve_
 *            underlying_call + _tool_search_scoped_names).
 *
 * This module is self-contained: it depends only on the shared core types
 * (tool_call_t / provider_response_t), libjson (for argument parsing), and
 * the registry (for Tool-Search scope checks). No god headers, opaque
 * struct, minimal includes. It is the single normalization point called by
 * llm_client.c for both non-streaming and streaming responses, replacing
 * the previous scattered per-provider behavior.
 */

#ifndef TOOL_RECOGNIZER_H
#define TOOL_RECOGNIZER_H

#include <stddef.h>
#include <stdbool.h>

#include "provider.h"          /* provider_response_t, tool_call_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Tool-Search bridge name, mirroring Python tools/tool_search.TOOL_CALL_NAME. */
#define TOOL_RECOGNIZER_BRIDGE_NAME "tool_call"

/* Options controlling normalization. All fields optional; pass zeros for
 * defaults (no scope enforcement, deterministic ids always on). */
typedef struct {
    /* If true, stage 5 Tool-Search unwrap is performed. Default: true. */
    bool unwrap_tool_search;
    /* If true, a call whose unwrapped underlying tool is not present in the
     * registry (out of session scope) is marked malformed with a scope error
     * instead of dispatching. Default: true. */
    bool enforce_scope;
} tool_recognizer_opts_t;

/* Default options (unwrap on, scope on). */
tool_recognizer_opts_t tool_recognizer_default_opts(void);

/* Normalize a parsed provider response in place.
 *
 * Applies, in order, for every tool_call in resp->tool_calls[0..count):
 *   - deterministic id fill (stage 2)
 *   - Tool-Search unwrap + scope check (stage 5)
 *   - malformed-arguments detection (stage 4)
 *
 * Call exactly once after a provider has populated resp (non-streaming
 * parse_response, or when a streaming response finalizes). Safe to call
 * once; calling twice is idempotent for ids/unwrap but re-validates args. */
void tool_recognizer_process_response(provider_response_t *resp,
                                       const tool_recognizer_opts_t *opts);

/* Single-call helpers (used by the streaming assembly path which builds
 * tool_calls incrementally). Each operates on one tool_call_t. */

/* Stage 2: ensure tc->id is set; synthesize if empty. */
void tool_recognizer_ensure_id(tool_call_t *tc, int index);

/* Stage 4: validate tc->arguments is a JSON object. Sets tc->malformed and
 * (if malformed) rewrites tc->arguments to the Hermes-style error result so
 * the dispatch loop can append it without executing the handler. */
void tool_recognizer_validate_args(tool_call_t *tc);

/* Stage 5: if tc->name is the Tool-Search bridge, peel it open to the
 * underlying tool. On success tc->name/arguments are replaced and tc->unwrapped
 * is set. If the underlying tool is not in registry scope (and scope enforced),
 * tc->malformed is set with a scope error. Returns true if an unwrap happened. */
bool tool_recognizer_unwrap_tool_search(tool_call_t *tc, bool enforce_scope);

#ifdef __cplusplus
}
#endif

#endif /* TOOL_RECOGNIZER_H */
