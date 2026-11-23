/*
 * local_viewer_events.c — faithful C11 port of the reasoning/MoA progress
 * payload projection in gateway/local_viewer_events.py
 * (gate w14_localviewe_d6b45a).
 * PoP: w14_localviewe_d6b45a_publish_progress @ gateway/local_viewer_events.py:publish_progress
 */
#include "noc_w14_localviewe_d6b45a.h"

#include <stdlib.h>
#include <string.h>

static json_t *no_emit(void) {
    json_t *o = json_object();
    json_set(o, "emit", json_bool(false));
    json_set(o, "event_type", json_null());
    json_set(o, "payload", json_null());
    return o;
}
static json_t *emit(const char *event_type, json_t *payload) {
    json_t *o = json_object();
    json_set(o, "emit", json_bool(true));
    json_set(o, "event_type", json_string(event_type));
    json_set(o, "payload", payload ? payload : json_object());
    return o;
}

/* Python truthiness of a JSON scalar (used for `payload.get("phase")`). */
static int py_truthy(json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
    case JSON_BOOL: return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && *v->str_val ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return json_len(v) != 0;
    default: return 0;
    }
}

/* `str(x or "")` with x injected as a C string (empty string is falsy). */
static const char *or_empty(const char *s) { return s ? s : ""; }

json_t *w14_localviewe_d6b45a_publish_progress(const char *event_type,
                                               const char *name,
                                               const char *preview,
                                               json_t *kwargs,
                                               int show_reasoning,
                                               int classic_cli) {
    const char *et = event_type ? event_type : "";
    const char *nm = or_empty(name);
    const char *pv = or_empty(preview);

    /* Classic CLI renders MoA references whatever its reasoning display;
     * reasoning.available itself needs show_reasoning. */
    int is_moa = !strcmp(et, "moa.reference") || !strcmp(et, "moa.progress") ||
                 !strcmp(et, "moa.phase") || !strcmp(et, "moa.aggregating");
    if (!strcmp(et, "subagent.text")) return no_emit();
    if (strcmp(et, "reasoning.available") && !is_moa) return no_emit();
    if (!(show_reasoning || (strcmp(et, "reasoning.available") && classic_cli)))
        return no_emit();

    json_t *payload = json_object();
    if (!strcmp(et, "reasoning.available")) {
        json_set(payload, "text", json_string(pv));
    } else if (!strcmp(et, "moa.reference")) {
        json_set(payload, "label", json_string(nm));
        json_set(payload, "text", json_string(pv));
    } else if (!strcmp(et, "moa.aggregating") || !strcmp(et, "moa.phase")) {
        if (*nm || !strcmp(et, "moa.aggregating"))
            json_set(payload, "aggregator", json_string(nm));
    } else { /* moa.progress */
        json_set(payload, "label", json_string(nm));
    }

    /* payload.update({out: kwargs[key] ...}) from _MOA_KEYS. */
    const char *const *keys = NULL;
    const char *const *outs = NULL;
    static const char *const ref_k[] = {"moa_index", "moa_count", NULL};
    static const char *const ref_o[] = {"index", "count", NULL};
    static const char *const prog_k[] = {"moa_refs_done", "moa_refs_total", NULL};
    static const char *const prog_o[] = {"refs_done", "refs_total", NULL};
    static const char *const phase_k[] = {"moa_phase", "moa_refs_done", "moa_refs_total", NULL};
    static const char *const phase_o[] = {"phase", "refs_done", "refs_total", NULL};
    if (!strcmp(et, "moa.reference")) { keys = ref_k; outs = ref_o; }
    else if (!strcmp(et, "moa.progress")) { keys = prog_k; outs = prog_o; }
    else if (!strcmp(et, "moa.phase")) { keys = phase_k; outs = phase_o; }
    /* moa.aggregating maps to {} */

    if (keys && kwargs && kwargs->type == JSON_OBJECT) {
        for (size_t i = 0; keys[i]; ++i) {
            json_t *v = json_obj_get(kwargs, keys[i]);
            if (v && v->type != JSON_NULL)
                json_set(payload, outs[i], json_copy(v));
        }
    }

    /* Post-update guards. */
    if (!strcmp(et, "moa.progress")) {
        if (!json_obj_get(payload, "refs_done") || !json_obj_get(payload, "refs_total")) {
            json_free(payload);
            return no_emit();
        }
    }
    if (!strcmp(et, "moa.phase")) {
        if (!py_truthy(json_obj_get(payload, "phase"))) {
            json_free(payload);
            return no_emit();
        }
    }
    return emit(et, payload);
}
