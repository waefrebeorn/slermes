/*
 * session_viewer_close.c — C port of upstream Hermes gateway/session_viewer_close.py.
 *
 * PoP: w16_sessionvie_e7b625_seconds @ gateway/session_viewer_close.py:_seconds
 * PoP: w16_sessionvie_e7b625_section @ gateway/session_viewer_close.py:_section
 * PoP: w16_sessionvie_e7b625_pending_reason @ gateway/session_viewer_close.py:_PendingClose.from_record
 * PoP: w16_sessionvie_e7b625_is_reset_reason @ gateway/session_viewer_close.py:_RESET_REASONS
 * PoP: w16_sessionvie_e7b625_is_orphan_reason @ gateway/session_viewer_close.py:ORPHAN_REASONS
 */
#include "noc_w16_sessionvie_e7b625.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* Python float(raw) under contextlib.suppress(ValueError, TypeError). */
static int float_from_json(const json_t *v, double *out) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        while (isspace((unsigned char)*s)) s++;
        if (!*s) return 0;
        char *end = NULL;
        double d = strtod(s, &end);
        if (end == s) return 0;
        while (isspace((unsigned char)*end)) end++;
        if (*end) return 0;
        *out = d;
        return 1;
    }
    return 0;
}

double w16_sessionvie_e7b625_seconds(const json_t *raw, double default_value) {
    double d;
    if (!float_from_json(raw, &d)) return default_value;
    return d < 0.0 ? 0.0 : d;   /* max(0.0, float(raw)) */
}

json_t *w16_sessionvie_e7b625_section(const json_t *config, const char *key) {
    if (config && config->type == JSON_OBJECT) {
        json_t *v = json_obj_get(config, key);
        if (v && v->type == JSON_OBJECT) return json_copy(v);
    }
    return json_object();
}

json_t *w16_sessionvie_e7b625_pending_reason(const char *record) {
    const char *rec = record ? record : "";
    if (!rec[0]) return json_string(rec);
    json_t *doc = json_parse(rec, NULL);
    if (!doc || doc->type != JSON_OBJECT) {
        if (doc) json_free(doc);
        return json_string(rec);             /* ValueError / TypeError / KeyError */
    }
    json_t *r = json_obj_get(doc, "reason");
    if (!r) { json_free(doc); return json_string(rec); }
    json_t *out = json_copy(r);              /* copy BEFORE json_free(doc) */
    json_free(doc);
    return out;
}

int w16_sessionvie_e7b625_is_reset_reason(const char *reason) {
    return (reason && (!strcmp(reason, "new_session") || !strcmp(reason, "tui_close")));
}

int w16_sessionvie_e7b625_is_orphan_reason(const char *reason) {
    return (reason && (!strcmp(reason, "ws_orphan_reap") || !strcmp(reason, "startup_orphan_reap")));
}

