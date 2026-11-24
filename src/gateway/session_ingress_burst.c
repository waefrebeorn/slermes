/*
 * session_ingress_burst.c — C port of upstream Hermes gateway/session_ingress_burst.py.
 *
 * PoP: w17_sessioning_11caf7_holds_busy_burst @ gateway/session_ingress_burst.py:holds_busy_burst
 * PoP: w17_sessioning_11caf7_serial @ gateway/session_ingress_burst.py:_serial
 * PoP: w17_sessioning_11caf7_kind @ gateway/session_ingress_burst.py:_kind
 */
#include "noc_w17_sessioning_11caf7.h"
#include "hermes_json.h"
#include <string.h>

/* Python truthiness for a JSON value (None/0/""/empty/false are falsy). */
static int s_falsy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 1;
    switch (v->type) {
        case JSON_BOOL:   return !v->bool_val;
        case JSON_NUMBER: return v->num_val == 0.0;
        case JSON_STRING: return !v->str_val || !v->str_val[0];
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count == 0;
        default: return 1;
    }
}
static int s_truthy(const json_t *v) { return !s_falsy(v); }

int w17_sessioning_11caf7_holds_busy_burst(const json_t *adapter, const char *session_key) {
    static const char *NAMES[2] = {"_shared_bursts", "_shared_burst_commits"};
    if (!session_key) return 0;
    for (size_t i = 0; i < 2; i++) {
        const json_t *v = adapter ? json_obj_get(adapter, NAMES[i]) : NULL;
        if (!v) continue;  /* adapter.__dict__.get(name, ()) -> () -> membership false */
        if (v->type == JSON_OBJECT) {
            /* `key in dict` tests the dict's keys. */
            if (json_obj_get(v, session_key) != NULL) return 1;
        } else if (v->type == JSON_ARRAY) {
            for (size_t j = 0; j < v->c.count; j++) {
                const json_t *e = v->c.items[j];
                if (e && e->type == JSON_STRING && e->str_val && !strcmp(e->str_val, session_key))
                    return 1;
            }
        }
        /* other types: `key in <non-iterable>` would raise in Python; fixtures keep dict/list/(). */
    }
    return 0;
}

int w17_sessioning_11caf7_serial(const json_t *adapter) {
    return s_truthy(adapter ? json_obj_get(adapter, "serial_receive_loop") : NULL) ? 1 : 0;
}

const char *w17_sessioning_11caf7_kind(const json_t *adapter, const json_t *event) {
    const char *mt = json_get_str(event, "message_type", "");
    int internal = json_get_bool(event, "internal", false) ? 1 : 0;
    if (mt && !strcmp(mt, "photo") && !internal) return "photo";
    /* adapter._is_queue_text_debounce_candidate(event) is scripted as a boolean flag. */
    return s_truthy(adapter ? json_obj_get(adapter, "_is_queue_text_debounce_candidate") : NULL)
           ? "text" : NULL;
}
