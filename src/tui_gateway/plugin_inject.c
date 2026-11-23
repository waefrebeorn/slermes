/*
 * plugin_inject.c — C home for upstream Hermes tui_gateway/plugin_inject.py.
 *
 * PoP: plugininj_inject_tui_session_message @
 *      tui_gateway/plugin_inject.py:inject_tui_session_message
 * Faithful routing decision; session store and background helpers are seams.
 */
#include "noc_w13_plugininje_70086e.h"
#include "hermes_json.h"

#include <string.h>

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
        default: return 0;
    }
}
static int is_ws(char c) { return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\f'||c=='\v'; }
static int blank_str(const char *s) {
    if (!s) return 1;
    for (; *s; ++s) if (!is_ws(*s)) return 0;
    return 1;
}

struct json_t *plugininj_inject_tui_session_message(const struct json_t *session_key,
                                                    const struct json_t *content,
                                                    const struct json_t *sessions) {
    int ok = 1;
    if (!session_key || session_key->type != JSON_STRING || !session_key->str_val || !session_key->str_val[0]) ok = 0;
    if (ok && (!content || content->type != JSON_STRING || blank_str(content->str_val))) ok = 0;

    const json_t *match = NULL;
    if (ok && sessions && sessions->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(sessions); ++i) {
            const json_t *s = json_get(sessions, i);
            if (!s || s->type != JSON_OBJECT) continue;
            const char *k = json_get_str(s, "session_key", NULL);
            if (k && session_key && !strcmp(k, session_key->str_val)) { match = s; break; }
        }
    }
    if (!match) return json_bool(false);

    if (py_truthy(json_obj_get(match, "lazy")) ||
        py_truthy(json_obj_get(match, "_closing")) ||
        py_truthy(json_obj_get(match, "_finalized"))) return json_bool(false);

    json_t *lock = json_obj_get(match, "history_lock");
    if (!lock || lock->type == JSON_NULL) return json_bool(false);

    /* running only decides queue-vs-drain; both paths return True. */
    (void)py_truthy(json_obj_get(match, "running"));
    return json_bool(true);
}
