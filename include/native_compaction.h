/*
 * native_compaction.h — Native OpenAI Responses server-side compaction gate.
 * Faithful port of agent/native_compaction.py.
 *
 * Pure decision/threshold logic for gpt-5.6-family server-side compaction.
 * The Python module reads attributes off an `agent` object; this C port takes
 * the resolved values explicitly (the caller supplies them), so there is no
 * agent-object dependency. Item-list restructuring (prune/merge) uses json_t.
 */

#ifndef NATIVE_COMPACTION_H
#define NATIVE_COMPACTION_H

#include <stdbool.h>
#include <stddef.h>

#include "json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NATIVE_COMPACTION_LOCAL_TRIGGER_SAFETY_MARGIN (8192)
#define NATIVE_COMPACTION_DEFAULT_COMPACT_THRESHOLD (200000)
#define NATIVE_COMPACTION_RETAINED_USER_MESSAGE_TOKEN_BUDGET (64000)
#define NATIVE_COMPACTION_ELIGIBLE_MODEL_MARKER "gpt-5.6"

/* True when the model is in the gpt-5.6 family (substring, lowercased). */
bool native_compaction_is_native_model(const char *model);

/* True for api.openai.com or the ChatGPT Codex backend — nothing else. */
bool native_compaction_is_direct_openai_route(const char *base_url, bool is_codex_backend);

/*
 * Clamp the configured native threshold below the local compressor trigger.
 * Returns an int (heap-free). `local_trigger_tokens` may be NULL (no
 * compressor attached).
 */
long native_compaction_resolve_threshold(const char *configured_threshold,
                                         const char *local_trigger_tokens);

/*
 * Return the context_management payload as a json_t array, or NULL.
 * NULL means "do not send the field". Caller frees the returned array (or
 * NULL). All gates are re-checked here.
 *
 * `codex_responses_native_compaction` / `compression_enabled` / `model` /
 * `base_url` / `codex_responses_compact_threshold` / `compressor_threshold`
 * are the resolved agent/compressor values.
 */
json_t *native_compaction_context_management(bool codex_responses_native_compaction,
                                             bool compression_enabled,
                                             const char *model,
                                             const char *base_url,
                                             bool is_codex_backend,
                                             bool is_xai_responses,
                                             bool is_github_responses,
                                             const char *codex_responses_compact_threshold,
                                             const char *compressor_threshold);

/* Cheap chars//4 token estimate. */
long native_compaction_approx_tokens(const char *text);

/* Extract the retained-budget text of a user-role input item (json object).
 * Returns a heap string (caller frees) or NULL. */
char *native_compaction_user_item_text(const json_t *item);

/*
 * Restructure Responses input around the newest compaction checkpoint.
 * `items` is a json array. Returns a NEW json array (caller frees) — or the
 * input array duplicated when no checkpoint is present. Self-gating.
 */
json_t *native_compaction_prune_pre_checkpoint_items(const json_t *items,
                                                     long retained_user_token_budget);

/* True when a provider error is a STRUCTURED rejection of the field. */
bool native_compaction_is_rejection(const char *error, const char *status_code);

/* Does this items array carry a compaction checkpoint? */
bool native_compaction_has_checkpoint(const json_t *items);

/*
 * Merge codex_reasoning_items across incomplete-continuation dedup,
 * preserving native compaction checkpoints. `prior`/`new_items` are json
 * arrays. Returns a NEW json array (caller frees).
 */
json_t *native_compaction_merge_interim_items(const json_t *prior,
                                              const json_t *new_items);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_COMPACTION_H */
