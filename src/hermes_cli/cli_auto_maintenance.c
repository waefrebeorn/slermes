/*
 * hermes_cli/cli_auto_maintenance.c -- real C11 port of upstream Hermes
 * hermes_cli/cli_auto_maintenance.py.
 *
 * PoP: w20_cliautomai_c467ab_run @ hermes_cli/cli_auto_maintenance.py:_run_state_db_auto_maintenance
 *
 * The pass is a pure control flow once the session-db handle, load_config() and
 * get_hermes_home() arrive as data: every refusal becomes a log record, and the
 * transcript it emits is the observable behaviour.
 */
#include "hermes_json.h"
#include "noc_w20_cliautomai_c467ab.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAM_G_DONE "Pruned %d empty TUI ghost sessions"
#define CAM_G_SKIP "Ghost session prune skipped: %s"
#define CAM_O_DONE "Finalized %d orphaned compression sessions"
#define CAM_O_SKIP "Orphan compression finalize skipped: %s"
#define CAM_SKIPPED "state.db auto-maintenance skipped: %s"

static char *cam_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static char *cam_cat3(const char *a, const char *b, const char *c) {
    size_t na, nb, nc;
    char *out;
    a = a ? a : "";
    b = b ? b : "";
    c = c ? c : "";
    na = strlen(a);
    nb = strlen(b);
    nc = strlen(c);
    out = (char *)malloc(na + nb + nc + 1);
    if (!out) return NULL;
    memcpy(out, a, na);
    memcpy(out + na, b, nb);
    memcpy(out + na + nb, c, nc + 1);
    return out;
}

static int cam_truthy(const json_t *v) {
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

/* type(x).__name__ for Python's error text. */
static const char *cam_type_name(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
    case JSON_NULL:   return "NoneType";
    case JSON_BOOL:   return "bool";
    case JSON_STRING: return "str";
    case JSON_ARRAY:  return "list";
    case JSON_OBJECT: return "dict";
    case JSON_NUMBER: return (floor(v->num_val) == v->num_val) ? "int" : "float";
    }
    return "object";
}

/* repr(str) -- enough for the int()/float() refusal texts, which quote it. */
static char *cam_repr(const char *s) {
    size_t n = strlen(s ? s : ""), i, j = 0;
    int has_single = 0, has_double = 0;
    char quote = '\'';
    char *out;
    out = (char *)malloc(2 * n + 3);
    if (!out) return cam_dup("");
    for (i = 0; i < n; i++) {
        if (s[i] == '\'') has_single = 1;
        if (s[i] == '"') has_double = 1;
    }
    if (has_single && !has_double) quote = '"';
    out[j++] = quote;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == (unsigned char)quote || c == '\\') out[j++] = '\\';
        if (c == '\n') { out[j++] = '\\'; c = 'n'; }
        else if (c == '\t') { out[j++] = '\\'; c = 't'; }
        else if (c == '\r') { out[j++] = '\\'; c = 'r'; }
        out[j++] = (char)c;
    }
    out[j++] = quote;
    out[j] = '\0';
    return out;
}

/* int(v) with Python's refusal texts. Returns NULL on success, else malloc'd msg. */
static char *cam_py_int(const json_t *v, long long *out) {
    char *msg;
    if (!v || v->type == JSON_NULL) {
        return cam_dup("int() argument must be a string, a bytes-like object or a real "
                       "number, not 'NoneType'");
    }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return NULL; }
    if (v->type == JSON_NUMBER) {
        if (!isfinite(v->num_val)) {
            char buf[256];
            snprintf(buf, sizeof(buf), "cannot convert float %s to integer",
                     v->num_val > 0 ? "infinity" : "NaN");
            return cam_dup(buf);
        }
        *out = (long long)v->num_val;
        return NULL;
    }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        const char *p = s;
        char *end;
        long long n;
        while (*p && isspace((unsigned char)*p)) p++;
        n = strtoll(p, &end, 10);
        if (end == p || *end) {
            char *rep = cam_repr(s);
            msg = cam_cat3("invalid literal for int() with base 10: ", rep, "");
            free(rep);
            return msg;
        }
        *out = n;
        return NULL;
    }
    {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "int() argument must be a string, a bytes-like object or a real number, "
                 "not '%s'", cam_type_name(v));
        return cam_dup(buf);
    }
}

/* float(v) with Python's refusal texts. Returns NULL on success. */
static char *cam_py_float(const json_t *v, double *out) {
    if (!v || v->type == JSON_NULL) {
        return cam_dup("float() argument must be a string or a real number, not 'NoneType'");
    }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return NULL; }
    if (v->type == JSON_NUMBER) { *out = v->num_val; return NULL; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        const char *p = s;
        char *end;
        double d;
        while (*p && isspace((unsigned char)*p)) p++;
        d = strtod(p, &end);
        if (end == p || *end) {
            char *rep = cam_repr(s);
            char *msg = cam_cat3("could not convert string to float: ", rep, "");
            free(rep);
            return msg;
        }
        *out = d;
        return NULL;
    }
    {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "float() argument must be a string or a real number, not '%s'",
                 cam_type_name(v));
        return cam_dup(buf);
    }
}

/* home / "sessions", posix-separated on both sides of the oracle. */
static char *cam_sessions_dir(const char *home) {
    size_t n, i;
    char *out;
    const char *h = home ? home : "";
    n = strlen(h);
    if (n == 0) return cam_dup("sessions");
    out = (char *)malloc(n + 10);
    if (!out) return cam_dup("sessions");
    for (i = 0; i < n; i++) out[i] = (h[i] == '\\') ? '/' : h[i];
    memcpy(out + n, "/sessions", 10);
    return out;
}

/* `fmt % value` for the module's single-placeholder log templates. */
static char *cam_fmt_s(const char *fmt, const char *value) {
    const char *at = strstr(fmt, "%s");
    size_t np, ns;
    char *out;
    if (!at) return cam_dup(fmt);
    np = (size_t)(at - fmt);
    ns = strlen(at + 2);
    out = (char *)malloc(np + strlen(value ? value : "") + ns + 1);
    if (!out) return cam_dup(fmt);
    memcpy(out, fmt, np);
    strcpy(out + np, value ? value : "");
    strcpy(out + np + strlen(value ? value : ""), at + 2);
    return out;
}

static char *cam_fmt_d(const char *fmt, long long value) {
    const char *at = strstr(fmt, "%d");
    size_t np, ns;
    char num[32];
    char *out;
    if (!at) return cam_dup(fmt);
    np = (size_t)(at - fmt);
    ns = strlen(at + 2);
    snprintf(num, sizeof(num), "%lld", value);
    out = (char *)malloc(np + strlen(num) + ns + 1);
    if (!out) return cam_dup(fmt);
    memcpy(out, fmt, np);
    strcpy(out + np, num);
    strcpy(out + np + strlen(num), at + 2);
    return out;
}

/* The repair() except tuple: sqlite3.Error, OSError, ValueError, RuntimeError. */
static int cam_caught_repair_error(const json_t *slot) {
    json_t *err = (slot && slot->type == JSON_OBJECT) ? json_obj_get(slot, "__raise__") : NULL;
    const char *t;
    if (!err) return 0;
    t = json_get_str(err, "type", "RuntimeError");
    if (!strcmp(t, "ValueError") || !strcmp(t, "RuntimeError") || !strcmp(t, "OSError")) return 1;
    if (!strncmp(t, "sqlite3.", 8)) return 1;
    return 0;
}

static void cam_log(json_t *events, const char *level, const char *msg) {
    json_t *e = json_object();
    json_set(e, "log", json_string(level));
    json_set(e, "msg", json_string(msg ? msg : ""));
    json_append(events, e);
}

static json_t *cam_event_call(const char *name) {
    json_t *e = json_object();
    json_set(e, "call", json_string(name));
    return e;
}

static void cam_call_noargs(json_t *events, const char *name) {
    json_t *e = cam_event_call(name);
    json_set(e, "kwargs", json_object());
    json_append(events, e);
}

static void cam_call_args(json_t *events, const char *name, const char *arg) {
    json_t *e = cam_event_call(name), *args = json_array();
    json_append(args, json_string(arg ? arg : ""));
    json_set(e, "args", args);
    json_append(events, e);
}

/* A slot carrying {"__raise__": {"type": ..., "message": ...}} -> malloc'd
 * str(exc); NULL when the slot does not refuse. */
static char *cam_raise_slot(const json_t *slot) {
    json_t *err, *msg;
    if (!slot || slot->type != JSON_OBJECT) return NULL;
    err = json_obj_get(slot, "__raise__");
    if (!err || err->type != JSON_OBJECT) return NULL;
    msg = json_obj_get(err, "message");
    if (msg && msg->type == JSON_STRING) return cam_dup(msg->str_val);
    return cam_dup("");
}

/* host[key] as a raise slot. */
static char *cam_raise_text(const json_t *host, const char *key) {
    if (!host || host->type != JSON_OBJECT) return NULL;
    return cam_raise_slot(json_obj_get(host, key));
}

/* The outer handler: one debug record and a stop. */
static json_t *cam_abort(json_t *events, const char *detail) {
    char *msg = cam_fmt_s(CAM_SKIPPED, detail ? detail : "");
    cam_log(events, "debug", msg);
    free(msg);
    return events;
}

static const char *cam_repair_key(int i) {
    return i == 0 ? "ghost_session_prune_v1" : "orphaned_compression_finalize_v1";
}

json_t *w20_cliautomai_c467ab_run(const json_t *db, const json_t *config, const char *home) {
    json_t *events = json_array();
    json_t *meta, *repairs;
    const char *home_s = home ? home : "";
    char *sessions_dir;
    json_t *cfg = NULL;
    int i;

    if (!db || db->type == JSON_NULL) return events;   /* session_db is None */
    if (db->type != JSON_OBJECT) {
        char buf[256];
        /* The loop tuple is built eagerly, and `session_db.finalize_orphaned_
         * compression_sessions` is the first bare attribute read on it. */
        snprintf(buf, sizeof(buf), "'%s' object has no attribute "
                 "'finalize_orphaned_compression_sessions'", cam_type_name(db));
        return cam_abort(events, buf);
    }
    meta = json_obj_get(db, "meta");
    repairs = json_obj_get(db, "repairs");
    sessions_dir = cam_sessions_dir(home_s);

    for (i = 0; i < 2; i++) {
        const char *key = cam_repair_key(i);
        char *err_text;
        json_t *slot, *count_v;
        long long count = 0;
        int have_count = 0;

        cam_call_args(events, "get_meta", key);
        if (cam_truthy(json_obj_get(meta, key))) continue;

        if (i == 0) {
            json_t *e = cam_event_call("prune_empty_ghost_sessions"), *kw = json_object();
            json_set(kw, "sessions_dir", json_string(sessions_dir));
            json_set(e, "kwargs", kw);
            json_append(events, e);
        } else {
            cam_call_noargs(events, "finalize_orphaned_compression_sessions");
        }
        slot = (repairs && repairs->type == JSON_OBJECT) ? json_obj_get(repairs, key) : NULL;
        err_text = cam_raise_slot(slot);
        if (err_text && !cam_caught_repair_error(slot)) {
            /* Not in the inner except tuple: the outer handler stops the pass. */
            free(sessions_dir);
            cam_abort(events, err_text);
            free(err_text);
            return events;
        }
        if (err_text) {
            const char *fmt = (i == 0) ? CAM_G_SKIP : CAM_O_SKIP;
            char *msg = cam_fmt_s(fmt, err_text);
            cam_log(events, "warning", msg);
            free(msg);
            free(err_text);
        } else {
            count_v = (slot && slot->type == JSON_OBJECT) ? json_obj_get(slot, "count") : NULL;
            if (cam_truthy(count_v)) {
                if (count_v->type == JSON_NUMBER) { count = (long long)count_v->num_val; have_count = 1; }
                else if (count_v->type == JSON_BOOL) { count = count_v->bool_val ? 1 : 0; have_count = 1; }
                else if (count_v->type == JSON_STRING) { count = strtoll(count_v->str_val, NULL, 10); have_count = 1; }
            }
        }
        {
            json_t *e = cam_event_call("set_meta"), *args = json_array();
            json_append(args, json_string(key));
            json_append(args, json_string("1"));
            json_set(e, "args", args);
            json_append(events, e);
        }
        if (have_count && count != 0) {
            char *msg = cam_fmt_d((i == 0) ? CAM_G_DONE : CAM_O_DONE, count);
            cam_log(events, "info", msg);
            free(msg);
        }
    }

    /* cfg = (load_config().get("sessions") or {}) */
    {
        char *err_text = cam_raise_slot(config);
        json_t *sessions;
        cam_call_noargs(events, "load_config");
        if (err_text) {
            free(sessions_dir);
            cam_abort(events, err_text);
            free(err_text);
            return events;
        }
        if (!config || config->type != JSON_OBJECT) {
            char buf[256];
            snprintf(buf, sizeof(buf), "'%s' object has no attribute 'get'",
                     cam_type_name(config));
            free(sessions_dir);
            return cam_abort(events, buf);
        }
        sessions = json_obj_get(config, "sessions");
        cfg = (sessions && sessions->type == JSON_OBJECT) ? sessions : NULL;
        if (sessions && sessions->type != JSON_OBJECT && cam_truthy(sessions)) {
            char buf[256];
            snprintf(buf, sizeof(buf), "'%s' object has no attribute 'get'",
                     cam_type_name(sessions));
            free(sessions_dir);
            return cam_abort(events, buf);
        }
    }

    if (cfg && cam_truthy(json_obj_get(cfg, "auto_archive"))) {
        char *err_text = cam_raise_text(json_obj_get(db, "calls"), "maybe_auto_archive");
        double idle_days = 3.0;
        long long min_hours = 24;
        json_t *days_v = cfg ? json_obj_get(cfg, "auto_archive_days") : NULL;
        json_t *hours_v = cfg ? json_obj_get(cfg, "min_interval_hours") : NULL;
        char *conv_err = NULL;
        if (err_text) {
            free(sessions_dir);
            cam_abort(events, err_text);
            free(err_text);
            return events;
        }
        if (days_v) conv_err = cam_py_float(days_v, &idle_days);
        if (!conv_err && hours_v) conv_err = cam_py_int(hours_v, &min_hours);
        if (conv_err) {
            free(sessions_dir);
            cam_abort(events, conv_err);
            free(conv_err);
            return events;
        }
        {
            json_t *e = cam_event_call("maybe_auto_archive"), *kw = json_object();
            json_set(kw, "idle_days", json_number(idle_days));
            json_set(kw, "min_interval_hours", json_number((double)min_hours));
            json_set(e, "kwargs", kw);
            json_append(events, e);
        }
    }

    if (!cfg || !cam_truthy(json_obj_get(cfg, "auto_prune"))) {
        free(sessions_dir);
        return events;
    }
    {
        char *err_text = cam_raise_text(json_obj_get(db, "calls"), "maybe_auto_prune_and_vacuum");
        long long retention = 90, hours = 24, vacuum_days = 30;
        int vacuum = 1;
        json_t *v;
        char *conv_err = NULL;
        json_t *vac;
        if (err_text) {
            free(sessions_dir);
            cam_abort(events, err_text);
            free(err_text);
            return events;
        }
        v = cfg ? json_obj_get(cfg, "retention_days") : NULL;
        if (v) conv_err = cam_py_int(v, &retention);
        if (!conv_err) {
            v = cfg ? json_obj_get(cfg, "min_interval_hours") : NULL;
            if (v) conv_err = cam_py_int(v, &hours);
        }
        if (!conv_err) {
            v = cfg ? json_obj_get(cfg, "min_vacuum_interval_days") : NULL;
            if (v) conv_err = cam_py_int(v, &vacuum_days);
        }
        vac = cfg ? json_obj_get(cfg, "vacuum_after_prune") : NULL;
        vacuum = vac ? cam_truthy(vac) : 1;
        if (conv_err) {
            free(sessions_dir);
            cam_abort(events, conv_err);
            free(conv_err);
            return events;
        }
        {
            json_t *e = cam_event_call("maybe_auto_prune_and_vacuum"), *kw = json_object();
            json_set(kw, "retention_days", json_number((double)retention));
            json_set(kw, "min_interval_hours", json_number((double)hours));
            json_set(kw, "min_vacuum_interval_days", json_number((double)vacuum_days));
            json_set(kw, "vacuum", json_bool(vacuum ? 1 : 0));
            json_set(kw, "sessions_dir", json_string(sessions_dir));
            json_set(e, "kwargs", kw);
            json_append(events, e);
        }
    }
    free(sessions_dir);
    return events;
}
