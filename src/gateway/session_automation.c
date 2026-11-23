/*
 * session_automation.c — C port of upstream Hermes gateway/session_automation.py.
 *
 * PoP: w13_sessionaut_846c76_identities @ gateway/session_automation.py:_identities
 * PoP: w13_sessionaut_846c76_queued_tick @ gateway/session_automation.py:_queued_tick
 * PoP: w13_sessionaut_846c76_notification_metadata @ gateway/session_automation.py:automation_notification_metadata
 * PoP: w13_sessionaut_846c76_admission_display @ gateway/session_automation.py:admission_display
 *
 * Pure identity/metadata helpers from the automation producer. Persistence,
 * adapters and async workers are replaced by JSON seams from the fixture.
 */
#include "noc_w13_sessionaut_846c76.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static int truthy_obj(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_object_size(v) > 0;
}

json_t *w13_sessionaut_846c76_identities(const struct json_t *raws) {
    json_t *out = json_array();
    if (!json_is_array(raws)) return out;
    size_t n = json_len(raws);
    for (size_t i = 0; i < n; i++) {
        const char *raw = jstr(json_get(raws, i));
        if (!raw) continue; /* json.loads(non-str) raises TypeError */
        json_t *id = json_parse(raw, NULL);
        if (json_is_array(id) && json_len(id) > 0) json_append(out, json_copy(id));
        json_free(id);
    }
    return out;
}

json_t *w13_sessionaut_846c76_queued_tick(const struct json_t *row,
                                           const struct json_t *process,
                                           const struct json_t *route) {
    int result = 0;
    if (!json_is_object(row)) return json_bool(0);
    const char *status = jstr(json_obj_get(row, "status"));
    if (!status || strcmp(status, "queued") != 0) return json_bool(0);
    const char *principal = jstr(json_obj_get(row, "principal_id"));
    const char *r = jstr(route);
    char expect[512];
    if (!principal || !r || strlen(r) + 11 >= sizeof expect) return json_bool(0);
    snprintf(expect, sizeof expect, "automation:%s", r);
    if (strcmp(principal, expect) != 0) return json_bool(0);
    const char *raw = jstr(json_obj_get(row, "request_id"));
    if (!raw) return json_bool(0);
    json_t *id = json_parse(raw, NULL);
    const char *p = jstr(process);
    if (json_is_array(id) && json_len(id) == 3) {
        const char *a = jstr(json_get(id, 0));
        const char *b = jstr(json_get(id, 1));
        if (a && b && !strcmp(a, "heartbeat") && p && !strcmp(b, p)) result = 1;
    }
    json_free(id);
    return json_bool(result);
}

json_t *w13_sessionaut_846c76_notification_metadata(const struct json_t *metadata) {
    const char *cat = "result";
    if (json_is_object(metadata)) {
        const json_t *v = json_obj_get(metadata, "notification_category");
        if (v) {
            if (v->type == JSON_NULL) cat = NULL; /* explicit None is not in the allowed set */
            else cat = jstr(v);
        }
    }
    if (!cat || (strcmp(cat, "result") != 0 && strcmp(cat, "diagnostic") != 0)) {
        json_t *err = json_object();
        json_set(err, "error", json_string("invalid_params"));
        return err;
    }
    json_t *out = json_object();
    if (!strcmp(cat, "diagnostic")) json_set(out, "notification_category", json_string("diagnostic"));
    return out;
}

json_t *w13_sessionaut_846c76_admission_display(const struct json_t *payload) {
    const json_t *d = NULL;
    if (json_is_object(payload)) {
        d = json_obj_get(payload, "local_automation_v1");
        if (!truthy_obj(d)) {
            const json_t *nt = json_obj_get(payload, "native_text_v1");
            const json_t *a = json_is_object(nt) ? json_obj_get(nt, "automation") : NULL;
            d = truthy_obj(a) ? a : NULL;
        }
    }
    const json_t *kind = d ? json_obj_get(d, "display_kind") : NULL;
    const json_t *text = d ? json_obj_get(d, "display_text") : NULL;
    const char *ks = jstr(kind);
    const char *ts = jstr(text);
    if (!ks || !*ks || !ts) return json_object();
    /* text.strip() must be non-empty */
    size_t tn = strlen(ts), a0 = 0, b0 = tn;
    while (a0 < b0 && isspace((unsigned char)ts[a0])) a0++;
    while (b0 > a0 && isspace((unsigned char)ts[b0 - 1])) b0--;
    if (a0 == b0) return json_object();
    const json_t *pt = json_is_object(payload) ? json_obj_get(payload, "text") : NULL;
    const char *body = jstr(pt);
    json_t *out = json_object();
    json_set(out, "display_kind", json_copy(kind));
    json_set(out, "display_text", json_copy(text));
    json_set(out, "text", json_string(body ? body : ""));
    const json_t *cat = json_obj_get(d, "notification_category");
    if (jstr(cat) && !strcmp(jstr(cat), "diagnostic"))
        json_set(out, "notification_category", json_string("diagnostic"));
    return out;
}

