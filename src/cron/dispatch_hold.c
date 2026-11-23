/*
 * dispatch_hold.c — C port of cron/dispatch_hold.py (gate w6_dispatchhol_96dc54).
 * PoP: w6_dispatchhol_96dc54_read @ cron/dispatch_hold.py:read
 * PoP: w6_dispatchhol_96dc54_grace_starts_at @ cron/dispatch_hold.py:grace_starts_at
 */
#include "hermes_json.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_dispatchhol_96dc54.h"

#define TICK_SLACK_SECONDS 60.0
#define MAX_EXTENSION_SECONDS 1800.0

/* Python float(value): number -> num, string -> strtod (whole string),
 * bool -> 1.0/0.0; anything else raises and makes read() return None. */
static int py_float(const struct json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    if (v->type == JSON_STRING && v->str_val) {
        const char *s = v->str_val;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        if (!*s) return 0;
        char *end = NULL;
        errno = 0;
        double d = strtod(s, &end);
        if (end == s) return 0;
        while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') end++;
        if (*end) return 0;
        *out = d;
        return 1;
    }
    return 0;
}

struct json_t *w6_dispatchhol_96dc54_read(const char *content) {
    if (!content) return NULL;
    /* read_text(encoding="utf-8-sig") drops a leading BOM. */
    if ((unsigned char)content[0] == 0xEF && (unsigned char)content[1] == 0xBB &&
        (unsigned char)content[2] == 0xBF)
        content += 3;
    char *err = NULL;
    json_t *data = json_parse(content, &err);
    if (!data || data->type != JSON_OBJECT) { json_free(data); return NULL; }
    const struct json_t *js = json_obj_get(data, "since");
    const struct json_t *jl = json_obj_get(data, "last");
    double since, last;
    if (!py_float(js, &since) || !py_float(jl, &last)) { json_free(data); return NULL; }
    json_free(data);
    struct json_t *out = json_object();
    json_set(out, "since", json_number(since));
    json_set(out, "last", json_number(last));
    return out;
}

double w6_dispatchhol_96dc54_grace_starts_at_ex(const struct json_t *held,
                                                double due_at, double grace) {
    if (!held || held->type != JSON_OBJECT) return due_at;
    double since = 0, last = 0;
    if (!py_float(json_obj_get(held, "since"), &since)) return due_at;
    if (!py_float(json_obj_get(held, "last"), &last)) return due_at;
    if (since - grace <= due_at && due_at <= last + TICK_SLACK_SECONDS &&
        due_at >= last - MAX_EXTENSION_SECONDS) {
        double cand = last + TICK_SLACK_SECONDS;
        return due_at > cand ? due_at : cand;   /* max(due_at, last + slack) */
    }
    return due_at;
}
