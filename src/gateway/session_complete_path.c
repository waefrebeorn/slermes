/*
 * session_complete_path.c — faithful C11 port of upstream
 * gateway/session_complete_path.py.
 *
 * The Ink TUI asks its canonical connection for composer completions rooted in
 * one session's frozen policy cwd. This module validates the declared params,
 * refuses unknown keys / mistyped values, checks the session exists, checks the
 * authorize() gate, and only lists a host-cwd backend's tree. Every runtime
 * collaborator arrives through the seam parameters, so the module's own
 * decision logic is reproduced exactly here.
 *
 * PoP: sesscomplete_complete @ gateway/session_complete_path.py:complete_path
 */
#include "hermes_json.h"
#include <string.h>
#include "noc_sesscomplete.h"

static json_t *refusal(const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string("AuthorityRefusal"));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}

static json_t *items_result(const json_t *items) {
    json_t *o = json_object();
    json_set(o, "items", items && items->type == JSON_ARRAY ? json_copy(items) : json_array());
    return o;
}

static int in_sessions(const json_t *sessions, const char *sid) {
    if (!sessions || sessions->type != JSON_ARRAY || !sid) return 0;
    for (size_t i = 0; i < sessions->c.count; i++) {
        json_t *v = sessions->c.items[i];
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, sid)) return 1;
    }
    return 0;
}

struct json_t *sesscomplete_complete(const json_t *params, const char *session_id,
                                     const json_t *sessions, int policy_null,
                                     int host_cwd, const json_t *items,
                                     const char *authorize_raise) {
    /* if (set(params) - {'session_id','profile','word','cwd'}
     *     or any(not isinstance(value, str) for value in params.values())):
     *     raise AuthorityRefusal('invalid_params') */
    if (!params || params->type != JSON_OBJECT) return refusal("invalid_params");
    for (size_t i = 0; i < params->c.count; i++) {
        const char *k = params->c.keys[i];
        if (!k || (strcmp(k, "session_id") && strcmp(k, "profile") &&
                   strcmp(k, "word") && strcmp(k, "cwd")))
            return refusal("invalid_params");
        json_t *v = params->c.items[i];
        if (!v || v->type != JSON_STRING) return refusal("invalid_params");
    }

    /* word = params.get('word', '') */
    json_t *word_v = json_obj_get(params, "word");
    const char *word = (word_v && word_v->type == JSON_STRING && word_v->str_val)
                       ? word_v->str_val : "";

    /* if not word or not ref.session_id: return {'items': []} */
    if (!word[0] || !session_id || !session_id[0]) return items_result(NULL);

    /* if ref.session_id not in authority.sessions: raise AuthorityRefusal('not_found') */
    if (!in_sessions(sessions, session_id)) return refusal("not_found");

    /* authorize(connection, ref, params, 'session:read') */
    if (authorize_raise && authorize_raise[0]) return refusal(authorize_raise);

    /* policy = policy_for_source(...)
     * if policy is None or not terminal_uses_host_cwd(...): return {'items': []} */
    if (policy_null || !host_cwd) return items_result(NULL);

    /* return {'items': await asyncio.to_thread(listing)} */
    return items_result(items);
}
