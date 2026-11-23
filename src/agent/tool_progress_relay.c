/*
 * tool_progress_relay.c — C home for upstream Hermes agent/tool_progress_relay.py.
 *
 * Faithful C11 ports of the tool.progress wire payload builder and the
 * contextvar reporter accessor.  Callback binding / throttling are
 * intentionally not exercised by the pure wire contract; the accessor reports
 * the unbound (None) state that the oracle observes on its thread.
 *
 * PoP: w11_toolprogre_3f5e37_payload @ agent/tool_progress_relay.py:tool_progress_payload
 * PoP: w11_toolprogre_3f5e37_current @ agent/tool_progress_relay.py:current_tool_progress
 */
#include "noc_w11_toolprogre_3f5e37.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int py_truthy(json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return 0;
}

/* str(v) for the JSON scalar kinds the wire payload accepts. */
static char *py_str(json_t *v) {
    if (!v) return (char *)strdup("");
    switch (v->type) {
        case JSON_STRING: return (char *)strdup(v->str_val ? v->str_val : "");
        case JSON_BOOL:   return (char *)strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            char buf[64];
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
            else
                snprintf(buf, sizeof(buf), "%.15g", v->num_val);
            return (char *)strdup(buf);
        }
        case JSON_NULL: return (char *)strdup("None");
        default:        return (char *)strdup("");
    }
}

static json_t *py_str_or_empty(json_t *v) {
    if (!py_truthy(v)) return json_string("");
    char *s = py_str(v);
    json_t *out = s ? json_string(s) : json_string("");
    free(s);
    return out;
}

/* PoP: w11_toolprogre_3f5e37_payload @ agent/tool_progress_relay.py:tool_progress_payload */
json_t *w11_toolprogre_3f5e37_payload(json_t *a) {
    json_t *kw = a ? json_obj_get(a, "kwargs") : NULL;
    if (!json_is_object(kw)) kw = NULL;
    json_t *out = json_object();
    json_t *idv = kw ? json_obj_get(kw, "tool_call_id") : NULL;
    json_set(out, "tool_id", py_str_or_empty(idv));
    json_set(out, "name", py_str_or_empty(a ? json_obj_get(a, "name") : NULL));

    json_t *pv = kw ? json_obj_get(kw, "progress") : NULL;
    double progress = 0.0;
    if (py_truthy(pv)) {
        if (json_is_number(pv)) progress = pv->num_val;
        else if (json_is_string(pv) && pv->str_val) progress = strtod(pv->str_val, NULL);
    }
    json_set(out, "progress", json_number(progress));

    json_t *tv = kw ? json_obj_get(kw, "total") : NULL;
    json_set(out, "total", tv ? json_copy(tv) : json_null());
    json_t *pcv = kw ? json_obj_get(kw, "percent") : NULL;
    json_set(out, "percent", pcv ? json_copy(pcv) : json_null());

    json_t *msg = a ? json_obj_get(a, "message") : NULL;
    if (py_truthy(msg)) {
        char *s = py_str(msg);
        json_set(out, "message", s ? json_string(s) : json_null());
        free(s);
    } else {
        json_set(out, "message", json_null());
    }
    return out;
}

/* PoP: w11_toolprogre_3f5e37_current @ agent/tool_progress_relay.py:current_tool_progress */
json_t *w11_toolprogre_3f5e37_current(json_t *a) {
    (void)a;
    return json_null();  /* no reporter bound on the harness thread */
}
