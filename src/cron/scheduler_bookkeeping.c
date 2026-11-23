/*
 * scheduler_bookkeeping.c — C home for upstream Hermes cron/scheduler_bookkeeping.py.
 * PoP: w13_schedulerb_e4b4e1_classify @ cron/scheduler_bookkeeping.py:_classify_delivery_outcome
 * PoP: w13_schedulerb_e4b4e1_fail_empty @ cron/scheduler_bookkeeping.py:fail_empty_response
 *
 * The two terminal bookkeeping decisions that are pure over their inputs: the
 * delivery-outcome classifier and the empty-response soft-failure guard.  Store,
 * ledger and journal side effects of finish_completed_run stay in Python.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_schedulerb_e4b4e1.h"

static const char *const sb_EMPTY = W13_SCHEDULERB_E4B4E1_EMPTY_RESPONSE_ERROR;

static int sb_truthy(json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    return v->c.count > 0;
}
static char *sb_dup(const char *s) { size_t n = strlen(s); char *o = (char *)malloc(n + 1); memcpy(o, s, n + 1); return o; }
/* Python str() for scalar final_response values (dict/list fall back to the
 * JSON serialization, which is not exercised by the guard's truthiness test). */
static char *sb_py_str(json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return sb_dup("");
    if (v->type == JSON_BOOL) return sb_dup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return sb_dup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        else snprintf(buf, sizeof(buf), "%g", v->num_val);
        return sb_dup(buf);
    }
    return sb_dup("");
}
static int sb_blank(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return *s == 0;
}

json_t *w13_schedulerb_e4b4e1_classify(json_t *delivery_error, json_t *should_deliver,
        json_t *unresolved_origin, json_t *normalized_deliver, json_t *incident_acked,
        json_t *success, json_t *delivery_queued, json_t *notification_suppressed) {
    const char *r;
    int sd = sb_truthy(should_deliver);
    if (sb_truthy(delivery_error)) r = "failed";
    else if (sd && sb_truthy(delivery_queued)) r = "queued";
    else if (sb_truthy(notification_suppressed)) r = "suppressed";
    else if (sd && sb_truthy(unresolved_origin)) r = "not_configured";
    else if (sd && !(normalized_deliver && normalized_deliver->type == JSON_STRING &&
                     normalized_deliver->str_val && !strcmp(normalized_deliver->str_val, "local")))
        r = "delivered";
    else if (sb_truthy(incident_acked) && !sb_truthy(success)) r = "suppressed_acked";
    else r = "suppressed";
    return json_string(r);
}

json_t *w13_schedulerb_e4b4e1_fail_empty(json_t *success, json_t *final_response, json_t *error) {
    json_t *out = json_object();
    int s = sb_truthy(success);
    /* `str(final_response or "")`: falsy values collapse to "". */
    json_t *v = sb_truthy(final_response) ? final_response : NULL;
    char *txt = sb_py_str(v);
    if (s && sb_blank(txt)) {
        json_set(out, "success", json_bool(0));
        json_set(out, "error", json_string(sb_EMPTY));
    } else {
        json_set(out, "success", json_bool(s ? 1 : 0));
        json_set(out, "error", error && error->type == JSON_STRING
                 ? json_string(error->str_val ? error->str_val : "") : (json_t *)json_null());
    }
    return out;
}
