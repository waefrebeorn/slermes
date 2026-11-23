/*
 * receipt.c — C port of upstream Hermes pm/receipt.py (gate w5_receipt_6c967d).
 *
 * PoP: w5_receipt_6c967d_receipt   @ pm/receipt.py:begin / record_* / snapshot
 * PoP: w5_receipt_6c967d_finalized @ pm/receipt.py:finalize / last_completed
 *
 * begin() stamps schema/kind/update_id/started_at/steps/venv_rebuild/
 * feature_list/platform/outcome/warnings/refusal. Each record_* mutates that
 * dict copy-on-write; finalize() adds outcome/exit_code/finished_at. Timestamps
 * use the fixed marker "T" so the differential oracle can compare them.
 */
#include "noc_w5_receipt_6c967d.h"

#include <stdlib.h>
#include <string.h>

#define AT "T"

static const char *jget(json_t *o, const char *k, const char *dflt) {
    json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_STRING) ? v->str_val : dflt;
}
static bool jbool(json_t *o, const char *k, bool dflt) {
    json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_BOOL) ? v->bool_val : dflt;
}
static json_t *jgetnode(json_t *o, const char *k) {
    return json_obj_get(o, k);
}

static json_t *new_receipt(const char *kind, const char *update_id) {
    json_t *r = json_object();
    json_set(r, "schema", json_number(1));
    json_set(r, "kind", json_string(kind ? kind : ""));
    json_set(r, "update_id", update_id ? json_string(update_id) : json_null());
    json_set(r, "started_at", json_string(AT));
    json_set(r, "steps", json_array());
    json_set(r, "venv_rebuild", json_null());
    json_set(r, "feature_list", json_null());
    json_set(r, "platform", json_null());
    json_set(r, "outcome", json_null());
    json_set(r, "warnings", json_array());
    json_set(r, "refusal", json_null());
    return r;
}

static void apply_ops(json_t *r, json_t *ops) {
    if (!ops || ops->type != JSON_ARRAY) return;
    size_t n = json_len(ops);
    for (size_t i = 0; i < n; i++) {
        json_t *op = json_get(ops, i);
        if (!op || op->type != JSON_OBJECT) continue;
        const char *kind = jget(op, "op", "");
        if (!strcmp(kind, "step")) {
            json_t *s = json_object();
            json_set(s, "name", json_string(jget(op, "name", "")));
            json_set(s, "ok", json_bool(jbool(op, "ok", false)));
            json_set(s, "detail", json_string(jget(op, "detail", "")));
            json_set(s, "at", json_string(AT));
            json_append(json_obj_get(r, "steps"), s);
        } else if (!strcmp(kind, "rebuild")) {
            json_t *v = json_object();
            json_set(v, "ok", json_bool(jbool(op, "ok", false)));
            json_set(v, "reason", json_string(jget(op, "reason", "")));
            json_obj_del(r, "venv_rebuild");
            json_set(r, "venv_rebuild", v);
        } else if (!strcmp(kind, "features")) {
            json_t *extras = jgetnode(op, "extras");
            json_obj_del(r, "feature_list");
            json_set(r, "feature_list", extras && extras->type == JSON_ARRAY
                     ? json_copy(extras) : json_null());
        } else if (!strcmp(kind, "platform")) {
            json_obj_del(r, "platform");
            json_set(r, "platform", json_string(jget(op, "id", "")));
        } else if (!strcmp(kind, "warning")) {
            json_t *w = json_object();
            json_set(w, "message", json_string(jget(op, "message", "")));
            json_set(w, "at", json_string(AT));
            json_append(json_obj_get(r, "warnings"), w);
        } else if (!strcmp(kind, "refusal")) {
            json_t *v = json_object();
            json_set(v, "code", json_string(jget(op, "code", "")));
            json_set(v, "detail", json_string(jget(op, "detail", "")));
            json_set(v, "at", json_string(AT));
            json_obj_del(r, "refusal");
            json_set(r, "refusal", v);
        }
    }
}

json_t *w5_receipt_6c967d_receipt(const char *kind, const char *update_id,
                                  json_t *ops) {
    json_t *r = new_receipt(kind, update_id);
    apply_ops(r, ops);
    return r;
}

json_t *w5_receipt_6c967d_finalized(const char *kind, const char *update_id,
                                    const char *outcome, int exit_code,
                                    json_t *ops) {
    json_t *r = new_receipt(kind, update_id);
    apply_ops(r, ops);
    json_obj_del(r, "outcome");
    json_set(r, "outcome", json_string(outcome ? outcome : ""));
    json_set(r, "exit_code", json_number((double)exit_code));
    json_set(r, "finished_at", json_string(AT));
    return r;
}
