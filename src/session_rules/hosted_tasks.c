/*
 * hosted_tasks.c — C port of upstream Hermes session_rules/hosted_tasks.py.
 *
 * PoP: w14_hostedtask_5005e2_validate_task_input @ session_rules/hosted_tasks.py:validate_task_input
 *
 * The self-contained task-input validator. Python ValueError is surfaced as
 * {"error": "<message>"} for the differential oracle. The other module
 * functions depend on persistence/sibling validators and are not ported here.
 */
#include "noc_w14_hostedtask_5005e2.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TASK_INPUT_EVENTS 128

static json_t *ht_err(const char *msg) {
    json_t *o = json_object();
    json_set(o, "error", json_string(msg ? msg : ""));
    return o;
}
static int ht_plain_int(const json_t *v) {
    return v && v->type == JSON_NUMBER && v->num_val == (double)(long long)v->num_val;
}

json_t *w14_hostedtask_5005e2_validate_task_input(const json_t *value) {
    if (!json_is_object(value) || json_object_size(value) != 2)
        return ht_err("invalid task input context");
    const json_t *wv = json_obj_get(value, "watermark");
    const json_t *sv = json_obj_get(value, "event_seqs");
    if (!wv || !sv) return ht_err("invalid task input context");
    if (!ht_plain_int(wv) || wv->num_val < 0)
        return ht_err("invalid task input watermark");
    if (!json_is_array(sv)) return ht_err("task input context exceeded its bound");
    size_t n = json_len(sv);
    if (n < 1 || n > MAX_TASK_INPUT_EVENTS)
        return ht_err("task input context exceeded its bound");
    double prev = wv->num_val;
    json_t *seqs = json_array();
    for (size_t i = 0; i < n; i++) {
        const json_t *e = json_get(sv, i);
        if (!ht_plain_int(e) || !(prev < e->num_val) || e->num_val >= 9223372036854775808.0) {
            json_free(seqs);
            return ht_err("task input sequences must be increasing after the watermark");
        }
        json_append(seqs, json_number(e->num_val));
        prev = e->num_val;
    }
    json_t *out = json_object();
    json_set(out, "watermark", json_number(wv->num_val));
    json_set(out, "event_seqs", seqs);
    return out;
}
