/*
 * session_bot_cancel.c — C port of upstream Hermes gateway/session_bot_cancel.py.
 *
 * PoP: w13_sessionbot_92b150_cancel @ gateway/session_bot_cancel.py:_cancel
 *
 * The live _cancel is driven in the oracle with scripted collaborators (delivery
 * id, receipt store, admission row, connection.cancel answer, receipt result).
 * This C port reproduces that exact decision tree over the same JSON seams.
 */
#include "noc_w13_sessionbot_92b150.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static int truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0];
        case JSON_NULL: return 0;
        default: return json_object_size(v) > 0 || (v->type == JSON_ARRAY && json_len(v) > 0);
    }
}
static json_t *err(const char *reason) {
    json_t *out = json_object();
    json_set(out, "error", json_string(reason));
    return out;
}
static int allowed_param(const char *k) {
    return !strcmp(k, "id") || !strcmp(k, "profile") || !strcmp(k, "author_id");
}

json_t *w13_sessionbot_92b150_cancel(const struct json_t *params,
                                     const struct json_t *record,
                                     const struct json_t *actor_subject,
                                     const struct json_t *retry_id,
                                     const struct json_t *row_status,
                                     const struct json_t *retry_eligible,
                                     const struct json_t *answer_status,
                                     const struct json_t *answer_outcome,
                                     const struct json_t *receipt_status) {
    if (!json_is_object(params)) return err("invalid_params");
    for (size_t i = 0; i < json_object_size(params); i++) {
        const char *k = json_object_get_key_at(params, i);
        if (!k || !allowed_param(k)) return err("invalid_params");
    }
    const char *did = jstr(json_obj_get(params, "id")); /* _delivery_id seam: non-empty str else ValueError */
    if (!did || !*did) return err("invalid_params");
    if (!json_is_object(record)) return err("not_found");
    const json_t *adm = json_obj_get(record, "admission_id");
    const char *admission = jstr(adm);
    if (!admission || !*admission) return err("not_found");
    const char *principal = jstr(json_obj_get(record, "principal_id"));
    const char *subject = jstr(actor_subject);
    if (!principal || !subject || strcmp(principal, subject) != 0) return err("permission_denied");
    const char *author_id = jstr(json_obj_get(json_obj_get(record, "author"), "id"));
    const char *want_author = jstr(json_obj_get(params, "author_id"));
    if ((!author_id && want_author) || (author_id && !want_author) ||
        (author_id && want_author && strcmp(author_id, want_author) != 0))
        return err("permission_denied");

    const char *rid = jstr(retry_id);
    const char *current = (rid && *rid) ? rid : admission;
    const char *rs = jstr(row_status);
    int live = rs && strcmp(rs, "terminal") != 0;
    int withdrawn = (!rid || !*rid) && (live || truthy(retry_eligible));
    const char *status = "finished";
    if (live) {
        status = "stopping";
        const char *as = jstr(answer_status);
        if (as && !strcmp(as, "terminal")) {
            const char *ao = jstr(answer_outcome);
            status = (ao && !strcmp(ao, "cancelled")) ? "cancelled" : "finished";
        }
    } else if (withdrawn) {
        status = "cancelled";
    }
    json_t *out = json_object();
    json_set(out, "delivery_id", json_string(did));
    json_set(out, "status", json_string(status));
    const char *receipt = jstr(receipt_status);
    json_set(out, "receipt_status", receipt ? json_string(receipt) : json_null());
    (void)current;
    return out;
}

