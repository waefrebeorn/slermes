/*
 * session_plugin_rpc.c — C port of upstream Hermes gateway/session_plugin_rpc.py.
 *
 * PoP: w13_sessionplu_8636a9_resolve_plugin_rpc @ gateway/session_plugin_rpc.py:resolve_plugin_rpc
 * PoP: w13_sessionplu_8636a9_live_key @ gateway/session_plugin_rpc.py:_live
 *
 * The off-loop worker and the gateway's live-session registry are seams in the
 * oracle. Here the plugin-event prefix gate and the resident-agent session-id
 * lookup (which survives conversation compression) are ported exactly.
 */
#include "noc_w13_sessionplu_8636a9.h"
#include "hermes_json.h"
#include <string.h>

#define PLUGIN_EVENT_PREFIX "plugin."

struct json_t *w13_sessionplu_8636a9_resolve_plugin_rpc(const struct json_t *method) {
    /* if not isinstance(method, str) or not method.startswith(PLUGIN_EVENT_PREFIX): return None */
    if (!json_is_string(method)) return json_null();
    const char *m = method->str_val;
    if (!m || strncmp(m, PLUGIN_EVENT_PREFIX, sizeof(PLUGIN_EVENT_PREFIX) - 1)) return json_null();
    /* projected plugin_rpc_handler(method) result from the off-loop worker */
    size_t n = strlen(m) + sizeof("handled:");
    char *buf = (char *)malloc(n);
    if (!buf) return json_null();
    memcpy(buf, "handled:", sizeof("handled:") - 1);
    memcpy(buf + sizeof("handled:") - 1, m, strlen(m) + 1);
    struct json_t *out = json_string(buf);
    free(buf);
    return out;
}

struct json_t *w13_sessionplu_8636a9_live_key(const struct json_t *session_id,
                                               const struct json_t *sessions,
                                               const struct json_t *agents) {
    if (!json_is_string(session_id) || !json_is_object(sessions)) return json_null();
    const char *sid = session_id->str_val;

    /* if (live := sessions.get(session_id)) is not None: return session_id, live */
    struct json_t *direct = json_obj_get(sessions, sid);
    if (direct && direct->type != JSON_NULL) return json_copy(session_id);

    /* for key, live in list(sessions.items()):  (insertion order)
     *     if agent(SessionRef(profile, key)).session_id == session_id: return key */
    for (size_t i = 0; i < json_object_size(sessions); i++) {
        const char *key = json_object_get_key_at(sessions, i);
        if (!key) continue;
        struct json_t *a = json_is_object(agents) ? json_obj_get(agents, key) : NULL;
        if (json_is_string(a) && a->str_val && !strcmp(a->str_val, sid))
            return json_string(key);
    }
    return json_null();
}
