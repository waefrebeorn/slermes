/*
 * session_active_list.c — faithful C11 port of the active-session row
 * projection in gateway/session_active_list.py (gate w14_sessionact_06a3c5).
 * PoP: w14_sessionact_06a3c5_status @ gateway/session_active_list.py:_status
 * PoP: w14_sessionact_06a3c5_active_list @ gateway/session_active_list.py:active_list
 */
#include "noc_w14_sessionact_06a3c5.h"

#include <stdio.h>
#include <string.h>

/* str(x or "") for the JSON scalars used by the live row projection. */
static void py_str_or_empty(json_t *v, char *buf, size_t cap) {
    buf[0] = '\0';
    if (!v) return;
    if (v->type == JSON_STRING) {
        if (v->str_val) snprintf(buf, cap, "%s", v->str_val);
    } else if (v->type == JSON_NUMBER) {
        if (v->num_val == 0.0) return;
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, cap, "%lld", (long long)v->num_val);
        else
            snprintf(buf, cap, "%g", v->num_val);
    } else if (v->type == JSON_BOOL && v->bool_val) {
        snprintf(buf, cap, "True");
    }
}
static double py_float_or(json_t *v, double dflt) {
    if (!v || v->type != JSON_NUMBER) return dflt;
    return v->num_val == 0.0 ? dflt : v->num_val;
}
static long long py_int_or(json_t *v, long long dflt) {
    if (!v || v->type != JSON_NUMBER) return dflt;
    return v->num_val == 0.0 ? dflt : (long long)v->num_val;
}
static int is_internal_source(const char *s) {
    return s && (!strcmp(s, "kanban") || !strcmp(s, "tool") || !strcmp(s, "oneshot"));
}

json_t *w14_sessionact_06a3c5_status(const char *execution_state, int asked,
                                     json_t *pending) {
    if (execution_state && !strcmp(execution_state, "running"))
        return json_string(asked ? "waiting" : "working");
    if (pending && pending->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(pending); ++i) {
            json_t *it = json_get(pending, i);
            if (it && it->type == JSON_STRING && it->str_val && !strcmp(it->str_val, "queued"))
                return json_string("starting");
        }
    }
    return json_string("idle");
}

static json_t *refusal(const char *msg) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(false));
    json_set(o, "error", json_string(msg));
    json_set(o, "sessions", json_null());
    return o;
}

json_t *w14_sessionact_06a3c5_active_list(json_t *params,
                                          json_t *capabilities,
                                          json_t *entries) {
    /* params keys must be a subset of {profile, current_session_id}. */
    if (!params || params->type != JSON_OBJECT) return refusal("invalid_params");
    for (size_t i = 0; i < json_object_size(params); ++i) {
        const char *k = json_object_get_key_at(params, i);
        if (!k || (strcmp(k, "profile") && strcmp(k, "current_session_id")))
            return refusal("invalid_params");
    }
    int can_read = 0;
    if (capabilities && capabilities->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(capabilities); ++i) {
            json_t *it = json_get(capabilities, i);
            if (it && it->type == JSON_STRING && it->str_val && !strcmp(it->str_val, "session:read")) {
                can_read = 1; break;
            }
        }
    }
    if (!can_read) return refusal("permission_denied");

    char current[512];
    py_str_or_empty(json_obj_get(params, "current_session_id"), current, sizeof(current));

    json_t *rows = json_array();
    if (entries && entries->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(entries); ++i) {
            json_t *e = json_get(entries, i);
            if (!e || e->type != JSON_OBJECT) continue;
            json_t *row = json_obj_get(e, "row");
            json_t *src_v = row ? json_obj_get(row, "source") : NULL;
            const char *src = (src_v && src_v->type == JSON_STRING) ? src_v->str_val : NULL;
            json_t *closed = json_obj_get(e, "closed");
            if (is_internal_source(src) || (closed && closed->type == JSON_BOOL && closed->bool_val))
                continue;

            char sid[512];
            py_str_or_empty(json_obj_get(e, "session_id"), sid, sizeof(sid));
            double started = py_float_or(row ? json_obj_get(row, "started_at") : NULL, 0.0);
            double last = py_float_or(row ? json_obj_get(row, "ended_at") : NULL, started);
            long long mc = py_int_or(row ? json_obj_get(row, "message_count") : NULL, 0);
            char model[512], title[512];
            py_str_or_empty(row ? json_obj_get(row, "model") : NULL, model, sizeof(model));
            json_t *title_v = row ? json_obj_get(row, "title") : NULL;
            py_str_or_empty(title_v, title, sizeof(title));

            json_t *status = w14_sessionact_06a3c5_status(
                json_get_str(e, "execution_state", ""),
                json_get_bool(e, "asked", false) ? 1 : 0,
                json_obj_get(e, "pending"));

            json_t *r = json_object();
            json_set(r, "current", json_bool(!strcmp(sid, current)));
            json_set(r, "id", json_string(sid));
            json_set(r, "session_key", json_string(sid));
            json_set(r, "last_active", json_number(last));
            json_set(r, "started_at", json_number(started));
            json_set(r, "message_count", json_number((double)mc));
            json_set(r, "model", json_string(model));
            json_set(r, "preview", json_string(""));
            json_set(r, "title", json_string(title));
            json_set(r, "source", src_v ? json_copy(src_v) : json_null());
            json_set(r, "status", status);
            json_append(rows, r);
        }
    }

    json_t *o = json_object();
    json_set(o, "ok", json_bool(true));
    json_set(o, "error", json_null());
    json_set(o, "sessions", rows);
    return o;
}
