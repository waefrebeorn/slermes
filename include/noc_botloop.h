#ifndef NOC_BOTLOOP_H
#define NOC_BOTLOOP_H

#include <stdbool.h>

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: botloop_as_bool @ gateway/bot_loop_guard.py:_as_bool */
bool botloop_as_bool(const struct json_t *raw, bool default_value);

/* PoP: botloop_as_positive @ gateway/bot_loop_guard.py:_as_positive */
double botloop_as_positive(const struct json_t *raw, double default_value);

/* PoP: botloop_as_positive_int @ gateway/bot_loop_guard.py:_as_positive_int */
long long botloop_as_positive_int(const struct json_t *raw, long long default_value);

/* PoP: botloop_configured_bot_user @ gateway/bot_loop_guard.py:configured_bot_user
 * `extra` is the platform config's `extra` mapping (or null). */
bool botloop_configured_bot_user(const struct json_t *extra, const struct json_t *user_id);

/* PoP: botloop_settings_from_config @ gateway/bot_loop_guard.py:settings_from_config
 * Takes the already-extracted gateway.bot_loop_guard block. */
struct json_t *botloop_settings_from_block(const struct json_t *block);

/* PoP: botloop_guard @ gateway/bot_loop_guard.py:BotLoopGuard.admit/chain_admit/note_reply/blocked
 * Runs op descriptors with injected settings and a per-op clock:
 *   {"op":"admit","conv":...,"t":...} -> [allowed,state]
 *   {"op":"blocked","conv":...,"t":...} -> bool
 *   {"op":"chain","conv":...,"sender":...,"text":...,"is_bot":...,"t":...} -> [allowed,state]
 *   {"op":"note","conv":...,"recipient":...} -> null
 * Returns a JSON array with one result per op. */
struct json_t *botloop_guard(const struct json_t *settings, const struct json_t *ops);

#ifdef __cplusplus
}
#endif

#endif /* NOC_BOTLOOP_H */
