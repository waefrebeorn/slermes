/*
 * noc_w32_methodscon_ce0f5d_p1.h - C surface for the validation half of
 * upstream tui_gateway/methods_config_set.py (gate w32_methodscon_ce0f5d_p1).
 *
 * What `config.set` decides before it touches state: how a client's `value`
 * becomes a word (Python ``str(value or "").strip().lower()``), which words
 * each key accepts, how the fast-mode alias table resolves, how /verbose
 * cycles, how `details_mode.<section>` parses its key path, and which setter
 * a key dispatches to.  Every config-file and agent write is a seam: the
 * functions return the effects they decided as the "writes" list.
 *
 * Every json_t* result is caller-owned (json_free).
 */
#ifndef NOC_W32_METHODSCON_CE0F5D_P1_H
#define NOC_W32_METHODSCON_CE0F5D_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _word(value) -> string */
json_t *w32_methodscon_ce0f5d_p1_normalize_word(const json_t *value);

/* _raw_word(value) -> string (only null is blank) */
json_t *w32_methodscon_ce0f5d_p1_normalize_raw_word(const json_t *value);

/* _BOOL_WORDS.get(str(value).strip().lower()) -> true | false | null */
json_t *w32_methodscon_ce0f5d_p1_bool_word(const json_t *value);

/* service_tier_word(tier) -> "fast" | "normal" | <tier> */
json_t *w32_methodscon_ce0f5d_p1_service_tier_word(const json_t *tier);

/* _set_fast(rid, params, "fast", value, None) with a selected model whose
 * overrides resolve: {"envelope": ..., "writes": [...]}. */
json_t *w32_methodscon_ce0f5d_p1_fast_set(const json_t *rid, const json_t *value,
                                          const json_t *current_tier);

/* _set_verbose: next cycle word (or 4002) from the caller's current mode. */
json_t *w32_methodscon_ce0f5d_p1_verbose_set(const json_t *rid, const json_t *value,
                                             const json_t *current);

/* _set_word / _word_setters: accept-or-refuse with the exact 4002 text and the
 * config effects the accepted word performs.  `key` may be the legacy
 * "approval_mode" alias (the envelope then reports "approvals.mode"). */
json_t *w32_methodscon_ce0f5d_p1_word_set(const json_t *rid, const json_t *key,
                                          const json_t *value);

/* _set_details_section: key path "details_mode.<section>", empty value drops
 * the per-section override. */
json_t *w32_methodscon_ce0f5d_p1_details_section(const json_t *rid, const json_t *key,
                                                 const json_t *value);

/* config.set key -> handler: {"handler": "_set_..."} or
 * {"kind": "error", "message": "unknown config key: X"}. */
json_t *w32_methodscon_ce0f5d_p1_resolve_key(const json_t *key);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W32_METHODSCON_CE0F5D_P1_H */
