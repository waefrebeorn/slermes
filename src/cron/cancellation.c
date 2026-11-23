/*
 * cancellation.c — C home for upstream Hermes cron/cancellation.py.
 *
 * PoP: w6_cancellation_b52bf7_valid_execution_id @ cron/cancellation.py:_marker_path
 * PoP: w6_cancellation_b52bf7_marker_name @ cron/cancellation.py:_marker_path
 * PoP: w6_cancellation_b52bf7_cancel_reason @ cron/cancellation.py:cancel_reason
 * PoP: w6_cancellation_b52bf7_cancel_run_none @ cron/cancellation.py:cancel_run
 * PoP: w6_cancellation_b52bf7_cancel_run @ cron/cancellation.py:cancel_run
 * PoP: w6_cancellation_b52bf7_combined_is_set @ cron/cancellation.py:_CombinedCancelEvent.is_set
 * PoP: w6_cancellation_b52bf7_combined_reason @ cron/cancellation.py:_CombinedCancelEvent.reason
 *
 * Marker-file I/O is replaced by an injected record/reason seam; the decision
 * logic and result shapes are ported verbatim.
 */
#include "noc_w6_cancellation_b52bf7.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_REASON "cancelled on request (hermes cron cancel)"

static char *trim_copy(const char *s)
{
    if (!s) return NULL;
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

int w6_cancellation_b52bf7_valid_execution_id(const char *execution_id)
{
    char *s = trim_copy(execution_id ? execution_id : "");
    if (!s) return 0;
    int ok = *s != '\0' && !strchr(s, '/') && !strchr(s, '\\') &&
             strcmp(s, ".") != 0 && strcmp(s, "..") != 0;
    free(s);
    return ok;
}

char *w6_cancellation_b52bf7_marker_name(const char *execution_id)
{
    char *s = trim_copy(execution_id ? execution_id : "");
    if (!s || !w6_cancellation_b52bf7_valid_execution_id(s)) { free(s); return NULL; }
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 8);
    if (out) snprintf(out, n + 8, "%s.cancel", s);
    free(s);
    return out;
}

const char *w6_cancellation_b52bf7_cancel_reason(const char *reason)
{
    return (reason && *reason) ? reason : "cron fire ownership was lost";
}

struct json_t *w6_cancellation_b52bf7_cancel_run_none(void)
{
    struct json_t *out = json_object();
    json_set(out, "success", json_bool(1));
    json_set(out, "cancelled", json_bool(0));
    json_set(out, "execution_id", json_null());
    json_set(out, "detail", json_string("No run of this job is in flight — nothing to cancel."));
    return out;
}

struct json_t *w6_cancellation_b52bf7_cancel_run(const struct json_t *record,
                                                 const char *reason)
{
    (void)reason;  /* injected seam: request_cancel() is the filesystem side effect */
    struct json_t *out = json_object();
    json_set(out, "success", json_bool(1));
    json_set(out, "cancelled", json_bool(1));
    const struct json_t *src = (record && record->type == JSON_OBJECT) ? record : NULL;
    const struct json_t *v;
    v = src ? json_obj_get(src, "id") : NULL;
    json_set(out, "execution_id", v ? json_copy(v) : json_null());
    v = src ? json_obj_get(src, "status") : NULL;
    json_set(out, "status", v ? json_copy(v) : json_null());
    v = src ? json_obj_get(src, "pid") : NULL;
    json_set(out, "pid", v ? json_copy(v) : json_null());
    json_set(out, "detail", json_string("Cancel requested; the run stops at its next cancel check."));
    return out;
}

int w6_cancellation_b52bf7_combined_is_set(const struct json_t *events)
{
    if (!events || events->type != JSON_ARRAY) return 0;
    size_t n = json_len(events);
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_get(events, i);
        if (e && e->type == JSON_OBJECT) {
            json_t *v = json_obj_get(e, "set");
            if (v && v->type == JSON_BOOL && v->bool_val) return 1;
        }
    }
    return 0;
}

char *w6_cancellation_b52bf7_combined_reason(const struct json_t *events)
{
    if (!events || events->type != JSON_ARRAY) return NULL;
    size_t n = json_len(events);
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_get(events, i);
        if (!e || e->type != JSON_OBJECT) continue;
        json_t *setv = json_obj_get(e, "set");
        if (!setv || setv->type != JSON_BOOL || !setv->bool_val) continue;
        json_t *rv = json_obj_get(e, "reason");
        if (rv && rv->type == JSON_STRING) return strdup(rv->str_val ? rv->str_val : "");
        return NULL;
    }
    return NULL;
}

