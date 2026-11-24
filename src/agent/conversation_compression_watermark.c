/*
 * agent/conversation_compression_watermark.c -- real C11 port of upstream
 * Hermes agent/conversation_compression_watermark.py.
 *
 * PoP: w20_conversati_6c3404_held_archive_watermark @ agent/conversation_compression_watermark.py:held_archive_watermark
 *
 * The watermark math the in-place compaction commit, the proactive prune and
 * micro-compaction share. Both store reads (the message role and the rewritten
 * held rows) arrive as a scripted stub, so the decision is pure.
 */
#include "hermes_json.h"
#include "noc_w20_conversati_6c3404.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* agent.context_compressor._DB_PERSISTED_MARKER (session_rules/turns.py). */
#define CCWM_DB_PERSISTED "_db_persisted"

static int ccwm_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* `type(x) is int and x > 0`: a bool is not an int here. */
static int ccwm_is_py_int(const json_t *v) {
    double d;
    if (!v || v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (!isfinite(d)) return 0;
    if (floor(d) != d) return 0;
    if (d < -9.007199254740992e15 || d > 9.007199254740992e15) return 0;
    return 1;
}

/* One `ids[]` slot: present (an exact row id) or None. */
typedef struct {
    int has;
    long long v;
} ccwm_slot;

/* _exact_id(m, copied): NULL when the id is not a positive int, or (for a
 * non-copied row) the dict carries no persist marker. */
static int ccwm_exact_id(const json_t *m, int copied, long long *out) {
    json_t *rid;
    if (!m || m->type != JSON_OBJECT) return 0;
    rid = json_obj_get(m, "_row_id");
    if (!ccwm_is_py_int(rid)) return 0;
    if (rid->num_val <= 0.0) return 0;
    if (!copied && !ccwm_truthy(json_obj_get(m, CCWM_DB_PERSISTED))) return 0;
    *out = (long long)rid->num_val;
    return 1;
}

static json_t *ccwm_ok(json_t *value) {
    json_t *out = json_object();
    json_set(out, "status", json_string("ok"));
    json_set(out, "value", value ? value : json_null());
    return out;
}

static json_t *ccwm_raised(const char *message) {
    json_t *out = json_object();
    json_set(out, "status", json_string("raised"));
    json_set(out, "type", json_string("StaleHeldHistory"));
    json_set(out, "message", json_string(message ? message : ""));
    return out;
}

/* The store-side lookup the module does through the seam. Returns 1 when the
 * rewrite list is non-empty (and writes its minimum), 0 for an empty/None one. */
static int ccwm_rewritten(const json_t *db, int persistence, long long *min_id) {
    json_t *host = db, *list;
    size_t i;
    int found = 0;
    long long best = 0;
    if (persistence) {
        json_t *p = (db && db->type == JSON_OBJECT) ? json_obj_get(db, "persistence") : NULL;
        host = p;
    }
    list = (host && host->type == JSON_OBJECT) ? json_obj_get(host, "rewritten") : NULL;
    if (!list || list->type != JSON_ARRAY) return 0;
    for (i = 0; i < list->c.count; i++) {
        json_t *v = list->c.items[i];
        long long id;
        if (!ccwm_is_py_int(v)) {
            if (v && v->type == JSON_STRING) {
                char *end;
                long long n = strtoll(v->str_val, &end, 10);
                if (end == v->str_val) continue;
                id = n;
            } else {
                continue;
            }
        } else {
            id = (long long)v->num_val;
        }
        if (!found || id < best) best = id;
        found = 1;
    }
    if (found) *min_id = best;
    return found;
}

/* get_message_role(session_id, newest_held): the role string, or NULL. */
static const char *ccwm_role(const json_t *db, int persistence, long long rid) {
    json_t *host = db, *roles, *v;
    char key[32];
    if (persistence) {
        json_t *p = (db && db->type == JSON_OBJECT) ? json_obj_get(db, "persistence") : NULL;
        host = p;
    }
    roles = (host && host->type == JSON_OBJECT) ? json_obj_get(host, "roles") : NULL;
    if (!roles || roles->type != JSON_OBJECT) return NULL;
    snprintf(key, sizeof(key), "%lld", rid);
    v = json_obj_get(roles, key);
    if (!v || v->type != JSON_STRING) return NULL;
    return v->str_val;
}

json_t *w20_conversati_6c3404_held_archive_watermark(const char *session_id,
                                                     const json_t *watermark,
                                                     const json_t *messages,
                                                     const json_t *verbatim_tail,
                                                     int stale_raises,
                                                     int persistence,
                                                     const json_t *db) {
    const char *sid = session_id ? session_id : "";
    long long wm = 0;
    int have_wm = 0;
    size_t nm, nt, i, nids = 0, cap;
    ccwm_slot *ids;
    long long newest = 0;
    int have_held = 0;
    const char *role;

    if (!watermark || watermark->type == JSON_NULL) return ccwm_ok(json_null());
    if (watermark->type == JSON_STRING) {
        char *end;
        long long v = strtoll(watermark->str_val, &end, 10);
        if (end != watermark->str_val) { wm = v; have_wm = 1; }
    } else if (ccwm_is_py_int(watermark)) {
        wm = (long long)watermark->num_val;
        have_wm = 1;
    }
    if (!have_wm) return ccwm_ok(json_null());

    nm = (messages && messages->type == JSON_ARRAY) ? messages->c.count : 0;
    nt = (verbatim_tail && verbatim_tail->type == JSON_ARRAY) ? verbatim_tail->c.count : 0;
    cap = nm + nt + 1;
    ids = (ccwm_slot *)calloc(cap, sizeof(ccwm_slot));
    if (!ids) return ccwm_ok(json_number((double)wm));

    /* ids = [_exact_id(m, False) for m in messages if isinstance(m, dict)]
     *     + [_exact_id(m, True) for m in (verbatim_tail or ()) if isinstance(m, dict)] */
    for (i = 0; i < nm; i++) {
        json_t *m = messages->c.items[i];
        long long v;
        if (!m || m->type != JSON_OBJECT) continue;
        ids[nids].has = ccwm_exact_id(m, 0, &v);
        ids[nids].v = v;
        nids++;
    }
    for (i = 0; i < nt; i++) {
        json_t *m = verbatim_tail->c.items[i];
        long long v;
        if (!m || m->type != JSON_OBJECT) continue;
        ids[nids].has = ccwm_exact_id(m, 1, &v);
        ids[nids].v = v;
        nids++;
    }

    for (i = 0; i < nids; i++) {
        if (!ids[i].has) continue;
        if (!have_held || ids[i].v > newest) newest = ids[i].v;
        have_held = 1;
    }

    if (stale_raises && have_held) {
        long long min_id = 0;
        if (ccwm_rewritten(db, persistence, &min_id)) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "held row %lld of session %s was rewritten in place", min_id, sid);
            free(ids);
            return ccwm_raised(msg);
        }
    }
    if (nids == 0 || !ids[nids - 1].has) {
        free(ids);
        return ccwm_ok(json_number((double)wm));
    }
    if (newest >= wm && !stale_raises) {
        free(ids);
        return ccwm_ok(json_number((double)wm));
    }
    role = ccwm_role(db, persistence, newest);
    if (!role) {
        if (stale_raises) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "held row %lld of session %s is no longer active", newest, sid);
            free(ids);
            return ccwm_raised(msg);
        }
        free(ids);
        return ccwm_ok(json_number((double)wm));
    }
    free(ids);
    return ccwm_ok(json_number((double)(newest < wm ? newest : wm)));
}
