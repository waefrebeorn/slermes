/*
 * kanban_watchers_dispatcher.c - C home for gateway/kanban_watchers_dispatcher.py.
 * Faithful port of the settings parser, the corrupt-board predicate, the quarantine
 * decision, the spawn-summary reducer and board-slug enumeration. DB/connect
 * collaborators stay seams (they are the live SQLite boundary).
 * PoP: w19_kanbanwatc_c182b8_resolve_settings @ gateway/kanban_watchers_dispatcher.py:_resolve_dispatcher_settings
 * PoP: w19_kanbanwatc_c182b8_is_corrupt_error @ gateway/kanban_watchers_dispatcher.py:_KanbanDispatcher.is_corrupt_board_db_error
 * PoP: w19_kanbanwatc_c182b8_quarantine_lifted @ gateway/kanban_watchers_dispatcher.py:_KanbanDispatcher._quarantine_lifted
 * PoP: w19_kanbanwatc_c182b8_log_spawn_results @ gateway/kanban_watchers_dispatcher.py:_log_spawn_results
 * PoP: w19_kanbanwatc_c182b8_board_slugs @ gateway/kanban_watchers_dispatcher.py:_KanbanDispatcher._board_slugs
 */
#include "noc_w19_kanbanwatc_c182b8.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define CORRUPT_BOARD_RETRY_AFTER_SECONDS 300.0

static char *dupstr(const char *s) {
    size_t n;
    char *p;
    if (!s) return NULL;
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (!p) return NULL;
    memcpy(p, s, n);
    return p;
}

static int py_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python str.strip() over the ASCII whitespace set. */
static char *py_strip_dup(const char *s) {
    const char *a;
    const char *b;
    size_t n;
    char *out;
    if (!s) return dupstr("");
    a = s;
    while (*a && py_ws((unsigned char)*a)) a++;
    b = a + strlen(a);
    while (b > a && py_ws((unsigned char)b[-1])) b--;
    n = (size_t)(b - a);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

/* Python truthiness of a JSON value. */
static int jobj_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:  return json_len(v) > 0 ? 1 : 0;
        case JSON_OBJECT: return json_object_size(v) > 0 ? 1 : 0;
        default:          return 0;
    }
}

/* Python int(str): optional ws, sign, decimal digits (single '_' between digits). */
static int py_int_from_str(const char *s, long long *out) {
    long long acc = 0;
    int neg = 0, digits = 0, prev_digit = 0;
    if (!s) return 0;
    while (*s && py_ws((unsigned char)*s)) s++;
    if (*s == '+' || *s == '-') { neg = (*s == '-'); s++; }
    while (*s) {
        if (*s >= '0' && *s <= '9') {
            acc = acc * 10 + (long long)(*s - '0');
            digits++;
            prev_digit = 1;
            s++;
        } else if (*s == '_' && prev_digit && s[1] >= '0' && s[1] <= '9') {
            prev_digit = 0;
            s++;
        } else {
            break;
        }
    }
    if (!digits) return 0;
    while (*s && py_ws((unsigned char)*s)) s++;
    if (*s != '\0') return 0;
    *out = neg ? -acc : acc;
    return 1;
}

/* Python int(v) for the JSON scalars these settings accept. */
static int py_int_coerce(const json_t *v, long long *out) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!(d == d) || d > 9.2e18 || d < -9.2e18) return 0;
        *out = (long long)d;  /* truncates toward zero, like int(float) */
        return 1;
    }
    if (v->type == JSON_STRING) return py_int_from_str(v->str_val, out);
    return 0;
}

/* Python float(v) for the JSON scalars; array/object/null raise TypeError. */
static int py_float_coerce(const json_t *v, double *out) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val;
        char *end = NULL;
        double d;
        const char *q;
        if (!s) return 0;
        for (q = s; *q; q++) {
            if (*q == 'x' || *q == 'X') return 0;  /* float() rejects hex-float text */
        }
        errno = 0;
        d = strtod(s, &end);
        if (end == s) return 0;
        while (*end && py_ws((unsigned char)*end)) end++;
        if (*end != '\0') return 0;
        *out = d;
        return 1;
    }
    return 0;
}

/* _positive_int_setting(kanban_cfg, key): None when unset/invalid or < 1. */
static int positive_int_setting(json_t *cfg, const char *key, long long *out) {
    json_t *raw = (cfg && cfg->type == JSON_OBJECT) ? json_obj_get(cfg, key) : NULL;
    long long v;
    if (!raw || raw->type == JSON_NULL) return 0;
    if (!py_int_coerce(raw, &v)) return 0;
    if (v < 1) return 0;
    *out = v;
    return 1;
}

static int jdeep_eq(const json_t *a, const json_t *b) {
    if (!a || !b) return a == b;
    if (a->type != b->type) return 0;
    switch (a->type) {
        case JSON_NULL:   return 1;
        case JSON_BOOL:   return (a->bool_val ? 1 : 0) == (b->bool_val ? 1 : 0);
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING:
            if (!a->str_val || !b->str_val) return a->str_val == b->str_val;
            return strcmp(a->str_val, b->str_val) == 0;
        case JSON_ARRAY: {
            size_t i;
            if (json_len(a) != json_len(b)) return 0;
            for (i = 0; i < json_len(a); i++)
                if (!jdeep_eq(json_get(a, i), json_get(b, i))) return 0;
            return 1;
        }
        case JSON_OBJECT: {
            size_t i;
            if (json_object_size(a) != json_object_size(b)) return 0;
            for (i = 0; i < json_object_size(a); i++) {
                const char *k = json_object_get_key_at(a, i);
                if (!jdeep_eq(json_object_get_at(a, i), json_obj_get(b, k))) return 0;
            }
            return 1;
        }
        default: return 0;
    }
}

/* ---- _resolve_dispatcher_settings(kanban_cfg, kb) -> asdict(_DispatcherSettings) ---- */
json_t *w19_kanbanwatc_c182b8_resolve_settings(json_t *kanban_cfg,
                                              long long default_failure_limit,
                                              json_t *effective_max_in_progress) {
    json_t *cfg = (kanban_cfg && kanban_cfg->type == JSON_OBJECT) ? kanban_cfg : NULL;
    json_t *out = json_object();
    json_t *raw;
    double interval;
    long long failure_limit;
    long long stale_timeout;
    long long per_profile;
    int enabled;

    /* interval = float(cfg.get("dispatch_interval_seconds", 60) or 60); max(interval, 1.0) */
    raw = cfg ? json_obj_get(cfg, "dispatch_interval_seconds") : NULL;
    if (jobj_truthy(raw)) {
        double parsed;
        interval = py_float_coerce(raw, &parsed) ? parsed : 60.0;
    } else {
        interval = 60.0;  /* `or 60`: unset, null, false, 0 or "" */
    }
    if (interval < 1.0) interval = 1.0;
    json_set(out, "interval", json_number(interval));

    /* max_spawn = cfg.get("max_spawn") - passthrough (None stays None). */
    raw = cfg ? json_obj_get(cfg, "max_spawn") : NULL;
    json_set(out, "max_spawn", raw ? json_copy(raw) : json_null());

    /* max_in_progress = _kbd().resolve_max_in_progress(_positive_int_setting(...)) */
    json_set(out, "max_in_progress",
             effective_max_in_progress ? json_copy(effective_max_in_progress) : json_null());

    /* failure_limit = int(cfg.get("failure_limit", kb.DEFAULT_FAILURE_LIMIT)); <1 or bad -> default */
    raw = cfg ? json_obj_get(cfg, "failure_limit") : NULL;
    if (!raw || !py_int_coerce(raw, &failure_limit)) failure_limit = default_failure_limit;
    if (failure_limit < 1) failure_limit = default_failure_limit;
    json_set(out, "failure_limit", json_number((double)failure_limit));

    /* stale = int(cfg.get("dispatch_stale_timeout_seconds", 0) or 0); bad -> 0 */
    raw = cfg ? json_obj_get(cfg, "dispatch_stale_timeout_seconds") : NULL;
    stale_timeout = 0;
    if (jobj_truthy(raw) && !py_int_coerce(raw, &stale_timeout)) stale_timeout = 0;
    json_set(out, "stale_timeout_seconds", json_number((double)stale_timeout));

    /* reconcile_orphans = bool(cfg.get("reconcile_orphans", True)) */
    raw = cfg ? json_obj_get(cfg, "reconcile_orphans") : NULL;
    enabled = raw ? jobj_truthy(raw) : 1;
    json_set(out, "reconcile_orphans", json_bool(enabled ? 1 : 0));

    /* default_assignee = (cfg.get("default_assignee") or "").strip() or None */
    raw = cfg ? json_obj_get(cfg, "default_assignee") : NULL;
    if (raw && raw->type == JSON_STRING && jobj_truthy(raw)) {
        char *trimmed = py_strip_dup(raw->str_val);
        if (trimmed && trimmed[0]) json_set(out, "default_assignee", json_string(trimmed));
        else json_set(out, "default_assignee", json_null());
        free(trimmed);
    } else {
        json_set(out, "default_assignee", json_null());
    }

    /* max_in_progress_per_profile = _positive_int_setting(cfg, "max_in_progress_per_profile") */
    if (positive_int_setting(cfg, "max_in_progress_per_profile", &per_profile))
        json_set(out, "max_in_progress_per_profile", json_number((double)per_profile));
    else
        json_set(out, "max_in_progress_per_profile", json_null());

    return out;
}

/* ---- is_corrupt_board_db_error(exc) ---- */
int w19_kanbanwatc_c182b8_is_corrupt_error(int kanban_corrupt, int sqlite_db_error,
                                           const char *msg) {
    static const char *const markers[] = {"file is not a database",
                                          "database disk image is malformed"};
    size_t i;
    char *lower;
    int hit = 0;
    if (kanban_corrupt) return 1;
    if (!sqlite_db_error) return 0;
    lower = dupstr(msg ? msg : "");
    if (!lower) return 0;
    for (i = 0; lower[i]; i++)
        if (lower[i] >= 'A' && lower[i] <= 'Z') lower[i] = (char)(lower[i] - 'A' + 'a');
    for (i = 0; i < sizeof(markers) / sizeof(markers[0]); i++)
        if (strstr(lower, markers[i])) hit = 1;
    free(lower);
    return hit;
}

/* ---- _quarantine_lifted(slug, fingerprint) ---- */
json_t *w19_kanbanwatc_c182b8_quarantine_lifted(int has_entry, json_t *disabled_fp,
                                                double disabled_at, json_t *fingerprint,
                                                double now_mono) {
    json_t *out = json_object();
    double age;
    int lifted;
    const char *logmsg;
    if (!has_entry) {
        json_set(out, "lifted", json_bool(1));
        json_set(out, "log", json_string("none"));
        json_set(out, "entry_after", json_bool(0));
        return out;
    }
    age = now_mono - disabled_at;
    if (jdeep_eq(disabled_fp, fingerprint) && age < CORRUPT_BOARD_RETRY_AFTER_SECONDS) {
        lifted = 0;
        logmsg = "none";
    } else if (jdeep_eq(disabled_fp, fingerprint)) {
        lifted = 1;
        logmsg = "unchanged";
    } else {
        lifted = 1;
        logmsg = "changed";
    }
    json_set(out, "lifted", json_bool(lifted));
    json_set(out, "log", json_string(logmsg));
    json_set(out, "entry_after", json_bool(lifted ? 0 : 1));  /* popped whenever it lifts */
    return out;
}

/* ---- _log_spawn_results(results) -> any_spawned ---- */
json_t *w19_kanbanwatc_c182b8_log_spawn_results(json_t *results) {
    json_t *out = json_object();
    int any = 0;
    size_t i;
    if (results && results->type == JSON_ARRAY) {
        for (i = 0; i < json_len(results); i++) {
            json_t *pair = json_get(results, i);
            json_t *res = NULL;
            if (pair && pair->type == JSON_ARRAY && json_len(pair) >= 2) res = json_get(pair, 1);
            if (res && res->type == JSON_OBJECT) {
                json_t *spawned = json_obj_get(res, "spawned");
                if (spawned && jobj_truthy(spawned)) any = 1;
            }
        }
    }
    json_set(out, "any_spawned", json_bool(any));
    return out;
}

/* ---- _board_slugs(kb): [b.get("slug") or DEFAULT_BOARD for b in boards] ---- */
json_t *w19_kanbanwatc_c182b8_board_slugs(json_t *boards, const char *default_board) {
    json_t *out = json_array();
    size_t i;
    if (boards && boards->type == JSON_ARRAY) {
        for (i = 0; i < json_len(boards); i++) {
            json_t *b = json_get(boards, i);
            json_t *slug = (b && b->type == JSON_OBJECT) ? json_obj_get(b, "slug") : NULL;
            if (slug && jobj_truthy(slug) && slug->type == JSON_STRING)
                json_append(out, json_string(slug->str_val));
            else
                json_append(out, json_string(default_board ? default_board : ""));
        }
    }
    return out;
}
