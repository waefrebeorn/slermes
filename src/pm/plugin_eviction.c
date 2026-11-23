/*
 * plugin_eviction.c — real C11 port of upstream Hermes pm/plugin_eviction.py.
 *
 * PoP: pluginevict_discard @ pm/plugin_eviction.py:_discard
 * PoP: pluginevict_list @ pm/plugin_eviction.py:_list
 * PoP: pluginevict_prune @ pm/plugin_eviction.py:_prune_record
 * PoP: pluginevict_settle @ pm/plugin_eviction.py:settle_record
 * PoP: pluginevict_notices @ pm/plugin_eviction.py:_outcome_notices
 *
 * RECORD == "disabled_by_update" (from pm.plugins_state) and
 * _RETRY == "a later update retries it" are mirrored from the live module.
 */
#include "noc_w6_pluginevict_1966c8.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RECORD "disabled_by_update"
#define RETRY_TEXT "a later update retries it"

static int jeq(const json_t *a, const json_t *b) {
    char *sa, *sb;
    int r;
    if (!a || !b) return a == b;
    sa = json_dumps(a, 0);
    sb = json_dumps(b, 0);
    r = sa && sb && !strcmp(sa, sb);
    free(sa); free(sb);
    return r;
}

static int jcontains(const json_t *arr, const json_t *item) {
    size_t i;
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (i = 0; i < json_len(arr); i++)
        if (jeq(json_get(arr, i), item)) return 1;
    return 0;
}

static int jcontains_str(const json_t *arr, const char *s) {
    size_t i;
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (i = 0; i < json_len(arr); i++) {
        json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, s))
            return 1;
    }
    return 0;
}

static void parent_of(const char *p, char *out, size_t cap) {
    const char *slash;
    size_t n;
    if (!p || !cap) { if (cap) out[0] = '\0'; return; }
    slash = strrchr(p, '/');
    if (!slash || slash == p) { snprintf(out, cap, "."); return; }
    n = (size_t)(slash - p);
    if (n >= cap) n = cap - 1;
    memcpy(out, p, n);
    out[n] = '\0';
}

static const char *map_get_str(const json_t *m, const char *k) {
    json_t *v = (m && m->type == JSON_OBJECT) ? json_obj_get(m, k) : NULL;
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static const char *arr_str(const json_t *a, size_t i, const char *def) {
    json_t *v = (a && a->type == JSON_ARRAY) ? json_get(a, i) : NULL;
    return (v && v->type == JSON_STRING) ? v->str_val : def;
}

static void prune_inplace(json_t *p) {
    json_t *rec, *disabled;
    size_t i;
    if (!p || p->type != JSON_OBJECT) return;
    rec = json_obj_get(p, RECORD);
    if (!rec || rec->type != JSON_OBJECT) { json_obj_del(p, RECORD); return; }
    disabled = json_obj_get(p, "disabled");
    for (i = json_object_size(rec); i > 0; i--) {
        const char *k = json_object_get_key_at(rec, i - 1);
        if (!k) continue;
        if (!jcontains_str(disabled, k)) json_obj_del(rec, k);
    }
    if (json_object_size(rec) == 0) json_obj_del(p, RECORD);
}

json_t *pluginevict_discard(const json_t *names, const json_t *spellings) {
    json_t *out;
    size_t i;
    if (!names || names->type != JSON_ARRAY) return json_array();
    out = json_copy(names);
    for (i = json_len(out); i > 0; i--)
        if (jcontains(spellings, json_get(out, i - 1))) json_array_remove(out, i - 1);
    return out;
}

json_t *pluginevict_list(const json_t *plugins, const char *key) {
    json_t *p, *v, *res;
    const char *k = key ? key : "";
    p = (plugins && plugins->type == JSON_OBJECT) ? json_copy(plugins) : json_object();
    if (!p || p->type != JSON_OBJECT) { json_free(p); p = json_object(); }
    v = json_obj_get(p, k);
    if (!v || v->type == JSON_NULL) {
        json_set(p, k, json_array());
        v = json_obj_get(p, k);
    }
    res = json_array();
    json_append(res, json_copy(v));
    json_append(res, p);
    return res;
}

json_t *pluginevict_prune(const json_t *plugins) {
    json_t *p = (plugins && plugins->type == JSON_OBJECT) ? json_copy(plugins) : json_object();
    if (!p || p->type != JSON_OBJECT) { json_free(p); p = json_object(); }
    prune_inplace(p);
    return p;
}

json_t *pluginevict_settle(const json_t *plugins, const json_t *settled) {
    json_t *p = (plugins && plugins->type == JSON_OBJECT) ? json_copy(plugins) : json_object();
    json_t *rec;
    if (!p || p->type != JSON_OBJECT) { json_free(p); p = json_object(); }
    rec = json_obj_get(p, RECORD);
    if (rec && rec->type == JSON_OBJECT && settled && settled->type == JSON_ARRAY) {
        size_t i;
        for (i = 0; i < json_len(settled); i++) {
            json_t *s = json_get(settled, i);
            if (s && s->type == JSON_STRING) json_obj_del(rec, s->str_val);
        }
    }
    prune_inplace(p);
    return p;
}

json_t *pluginevict_notices(const json_t *facts) {
    json_t *out = json_array();
    const json_t *entries, *recorded, *reasons, *waiting, *joined;
    char parent[1024], buf[8192];
    size_t i;

    if (!facts || facts->type != JSON_OBJECT) return out;
    entries  = json_obj_get(facts, "entries");
    recorded = json_obj_get(facts, "recorded");
    reasons  = json_obj_get(facts, "reasons");
    waiting  = json_obj_get(facts, "waiting");
    joined   = json_obj_get(facts, "joined");

    if (entries && entries->type == JSON_ARRAY) {
        for (i = 0; i < json_len(entries); i++) {
            json_t *e = json_get(entries, i);
            const char *name, *pldir;
            const char *reason, *wait;
            if (!e || e->type != JSON_ARRAY || json_len(e) < 2) continue;
            name = arr_str(e, 1, "");
            pldir = arr_str(e, 2, "");
            parent_of(arr_str(e, 0, ""), parent, sizeof(parent));
            reason = map_get_str(reasons, pldir);
            wait = map_get_str(waiting, pldir);
            if (reason) {
                snprintf(buf, sizeof(buf),
                    "Disabled plugin '%s' in %s: %s; %s and turns it back on once it fits, "
                    "or run `hermes plugins enable %s` sooner",
                    name, parent, reason, RETRY_TEXT, name);
            } else if (wait) {
                snprintf(buf, sizeof(buf),
                    "Left plugin '%s' in %s out of this update: %s; it stays enabled and "
                    "rejoins once Hermes reports a version it accepts",
                    name, parent, wait);
            } else {
                continue;
            }
            json_append(out, json_string(buf));
        }
    }

    if (recorded && recorded->type == JSON_ARRAY) {
        for (i = 0; i < json_len(recorded); i++) {
            json_t *pair = json_get(recorded, i);
            json_t *entry, *rec;
            const char *name, *pldir, *why;
            if (!pair || pair->type != JSON_OBJECT) continue;
            entry = json_obj_get(pair, "entry");
            rec = json_obj_get(pair, "record");
            if (!entry || entry->type != JSON_ARRAY || json_len(entry) < 2) continue;
            name = arr_str(entry, 1, "");
            pldir = arr_str(entry, 2, "");
            parent_of(arr_str(entry, 0, ""), parent, sizeof(parent));
            if (joined && jcontains(joined, entry)) {
                snprintf(buf, sizeof(buf),
                         "Turned plugin '%s' in %s back on: it fits this Hermes again",
                         name, parent);
            } else {
                why = map_get_str(reasons, pldir);
                if (!why) why = map_get_str(waiting, pldir);
                if (!why) why = map_get_str(rec, "reason");
                if (!why || !*why) why = "unknown";
                snprintf(buf, sizeof(buf),
                         "Plugin '%s' in %s is still disabled: %s; %s",
                         name, parent, why, RETRY_TEXT);
            }
            json_append(out, json_string(buf));
        }
    }
    return out;
}
