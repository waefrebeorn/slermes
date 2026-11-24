#ifndef NOC_W18_SESSIONGRO_0639E1_H
#define NOC_W18_SESSIONGRO_0639E1_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of gateway/session_group_controls.py (gate w18_sessiongro_0639e1).
 *
 * Only the module's pure tables, validators and payload/decision shapers are
 * exposed here; the dispatch entry points take a live AuthorityConnection, a
 * live store handle and a running hosted-room service, so they stay in Python.
 */

/* PoP: w18_sessiongro_0639e1_capability_for_method @ gateway/session_group_controls.py:GROUP_METHODS
 * GROUP_METHODS.get(method, 'session:read').  Never NULL: an unknown method
 * still answers the read capability, exactly like dict.get's default. */
const char *w18_sessiongro_0639e1_capability_for_method(const char *method);

/* PoP: w18_sessiongro_0639e1_fields_for_method @ gateway/session_group_controls.py:_FIELDS
 * The method's accepted parameter names (Python set -> sorted array), or null
 * when the method is not in _FIELDS. */
json_t *w18_sessiongro_0639e1_fields_for_method(const char *method);

/* PoP: w18_sessiongro_0639e1_params_allowed @ gateway/session_group_controls.py:dispatch_group_control
 * `method in _FIELDS and not set(params) - (_FIELDS[method] | {'profile'})`.
 * `params` must be the JSON object supplied on the wire; anything else (NULL,
 * a non-object) is not a dict and therefore refused. */
int w18_sessiongro_0639e1_params_allowed(const char *method, const json_t *params);

/* PoP: w18_sessiongro_0639e1_refusal @ gateway/session_group_controls.py:_refusal
 * `{"reason": reason}` plus `"admitted": false` only when the supplied admitted
 * value *is* boolean False (0, "", null and absence are not False by identity). */
json_t *w18_sessiongro_0639e1_refusal(const char *reason, const json_t *admitted);

/* PoP: w18_sessiongro_0639e1_ui_meta_revisions @ gateway/session_group_controls.py:_ui_meta_revisions
 * str keys, non-bool ints only, floored at 0; {} for anything else. */
json_t *w18_sessiongro_0639e1_ui_meta_revisions(const json_t *meta);

/* PoP: w18_sessiongro_0639e1_stat_key @ gateway/session_group_controls.py:_stat_key
 * (st_mtime_ns, st_ctime_ns, st_size, st_ino) as four decimal strings; the
 * nanosecond fields exceed JSON's double precision, so they travel as text. */
json_t *w18_sessiongro_0639e1_stat_key(const char *mtime_ns, const char *ctime_ns,
                                       const char *size, const char *ino);

/* PoP: w18_sessiongro_0639e1_has_avatar @ gateway/session_group_controls.py:_has_avatar
 * any(home/'assets'/f'avatar.{png,jpg,webp}').is_file(). */
int w18_sessiongro_0639e1_has_avatar(const char *home);

/* PoP: w18_sessiongro_0639e1_execution_control @ gateway/session_group_controls.py:_execution_control
 * Validation and receipt shaping for groups.continue / .retry / .discard /
 * .approve, against a scripted service:
 *   service = {"event": ...}                       (groups.continue)
 *   service = {"task": {...}}                      (groups.retry/.discard)
 *   service = {"result": ...} | {"error": {"type": ..., "message": ...}}
 * Answer: {"status":"ok","value":...} |
 *         {"status":"refused","reason":...,"admitted":null|false} |
 *         {"status":"raised","type":...,"message":...}. */
json_t *w18_sessiongro_0639e1_execution_control(const char *method, const json_t *params,
                                                const json_t *service);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_SESSIONGRO_0639E1_H */
