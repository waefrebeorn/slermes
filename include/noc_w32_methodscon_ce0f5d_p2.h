/*
 * noc_w32_methodscon_ce0f5d_p2.h - C surface for the payload/precedence half
 * of upstream tui_gateway/methods_config_set.py (gate w32_methodscon_ce0f5d_p2).
 *
 * The JSON-RPC envelopes every config.set answer is shaped into, the
 * confirm_required rule, the /reasoning display-word table, the on/off/mode
 * toggles with their flip precedence against the current display config, and
 * the stale-session predicate that stops a sessionless write from widening
 * scope.  Nothing here reads a file, an agent or the process env.
 *
 * Every json_t* result is caller-owned (json_free).
 */
#ifndef NOC_W32_METHODSCON_CE0F5D_P2_H
#define NOC_W32_METHODSCON_CE0F5D_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* server.py _err(rid, code, msg) */
json_t *w32_methodscon_ce0f5d_p2_err(const json_t *rid, const json_t *code, const json_t *message);

/* server.py _ok(rid, result) */
json_t *w32_methodscon_ce0f5d_p2_ok(const json_t *rid, const json_t *result);

/* _kv(rid, key, value, **extra) */
json_t *w32_methodscon_ce0f5d_p2_kv(const json_t *rid, const json_t *key, const json_t *value,
                                    const json_t *extra);

/* _cfgset_model_ok: confirm_required follows confirm_message. */
json_t *w32_methodscon_ce0f5d_p2_model_ok(const json_t *rid, const json_t *key,
                                          const json_t *value, const json_t *warning,
                                          const json_t *confirm_message, const json_t *scope,
                                          const json_t *extra);

/* bool(confirm_message) */
json_t *w32_methodscon_ce0f5d_p2_confirm_required(const json_t *confirm_message);

/* _set_reasoning with no live session: the display-word table, the accepted
 * effort set, and the config writes each performs. */
json_t *w32_methodscon_ce0f5d_p2_reasoning(const json_t *rid, const json_t *value,
                                           const json_t *scope);

/* _set_toggle: alias word, toggle flip against `display`, and the reported
 * word, plus the write.  `display` is the display.<field> config object. */
json_t *w32_methodscon_ce0f5d_p2_toggle_set(const json_t *rid, const json_t *key,
                                            const json_t *value, const json_t *display);

/* _set_display_toggle / _set_daybreak: `cfg_key` is what the setter writes
 * (null -> the dispatch key itself). */
json_t *w32_methodscon_ce0f5d_p2_bool_set(const json_t *rid, const json_t *key,
                                          const json_t *value, const json_t *cfg_key);

/* _set_prompt: "clear" empties the stored prompt, anything else is stored and
 * echoed verbatim. */
json_t *w32_methodscon_ce0f5d_p2_prompt_set(const json_t *rid, const json_t *value);

/* config.set's stale-session guard, as a predicate. */
json_t *w32_methodscon_ce0f5d_p2_is_stale_session(const json_t *key, const json_t *session_id_present,
                                                  const json_t *session_exists, const json_t *scope);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W32_METHODSCON_CE0F5D_P2_H */
