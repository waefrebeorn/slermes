/*
 * context_compressor_durable.c -- real C11 port of the bound session row's
 * durable compressor state: agent/context_compressor_durable.py.
 *
 * PoP: w20_contextcom_7fc1af_verb_table @ agent/context_compressor_durable.py:_DURABLE_VERBS
 * PoP: w20_contextcom_7fc1af_resolve_verb @ agent/context_compressor_durable.py:_durable_verb
 * PoP: w20_contextcom_7fc1af_durable_read @ agent/context_compressor_durable.py:_durable_read
 * PoP: w20_contextcom_7fc1af_durable_write @ agent/context_compressor_durable.py:_durable_write
 * PoP: w20_contextcom_7fc1af_load_durable @ agent/context_compressor_durable.py:_load_durable
 *
 * Everything the mixin touches besides its own logic -- the writer handle, the
 * session store and the store's verb callables -- is injected by the caller as
 * a scripted handle, exactly like the reference gate injects a scripted
 * service.  The never-raise, always-default contract is the ported behaviour.
 */
#include "hermes_json.h"
#include "noc_w20_contextcom_7fc1af.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* _DURABLE_VERBS: (repository, verb) per durable method. */
typedef struct {
    const char *method;
    const char *repository;
    const char *verb;
} ccd7fc1_verb_row;

static const ccd7fc1_verb_row CCD7FC1_VERBS[] = {
    {"get_compression_fallback_streak",      "lineage",  "compression_fallback_streak"},
    {"get_compression_ineffective_count",    "lineage",  "compression_ineffective_count"},
    {"get_compression_overload_streak",      "lineage",  "compression_overload_streak"},
    {"get_compression_recovery_deadline",    "lineage",  "compression_recovery_deadline"},
    {"increment_compression_overload_streak", "lineage", "increment_compression_overload_streak"},
    {"set_compression_fallback_streak",      "lineage",  "set_compression_fallback_streak"},
    {"set_compression_ineffective_count",    "lineage",  "set_compression_ineffective_count"},
    {"set_compression_overload_streak",      "lineage",  "set_compression_overload_streak"},
    {"set_compression_recovery_deadline",    "lineage",  "set_compression_recovery_deadline"},
    {"get_session_model_config_value",       "sessions", "get_model_config_value"},
    {"patch_session_model_config",           "sessions", "patch_model_config"},
    {"record_compression_failure_cooldown",  "messages", "record_compression_cooldown"},
    {"clear_compression_failure_cooldown",   "messages", "clear_compression_cooldown"},
    {NULL, NULL, NULL}
};

static const ccd7fc1_verb_row *ccd7fc1_find_verb(const char *method) {
    int i;
    if (!method) return NULL;
    for (i = 0; CCD7FC1_VERBS[i].method; i++) {
        if (!strcmp(method, CCD7FC1_VERBS[i].method)) return &CCD7FC1_VERBS[i];
    }
    return NULL;
}

json_t *w20_contextcom_7fc1af_verb_table(void) {
    json_t *out = json_object();
    int i;
    for (i = 0; CCD7FC1_VERBS[i].method; i++) {
        json_t *row = json_object();
        json_set(row, "repository", json_string(CCD7FC1_VERBS[i].repository));
        json_set(row, "verb", json_string(CCD7FC1_VERBS[i].verb));
        json_set(out, CCD7FC1_VERBS[i].method, row);
    }
    return out;
}

/* Which object a durable call resolves to. */
typedef enum {
    CCD7FC1_VIA_STORE,      /* a writer handle: the interface verb */
    CCD7FC1_VIA_DB,         /* no handle: session_db's own method */
    CCD7FC1_VIA_NONE        /* no handle and the store has no such method */
} ccd7fc1_via;

typedef struct {
    ccd7fc1_via via;
    const ccd7fc1_verb_row *row;
} ccd7fc1_resolved;

static const char *ccd7fc1_str(const json_t *obj, const char *key) {
    json_t *v;
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    v = json_obj_get(obj, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

/* `store is not None` decides the branch; only the interface's shape matters. */
static int ccd7fc1_store_bound(const json_t *req) {
    const char *s = ccd7fc1_str(req, "store");
    return s && !strcmp(s, "interface");
}

/* `session_db is None or getattr(session_db, method, None) is None`. */
static int ccd7fc1_db_has_method(const json_t *req) {
    json_t *v;
    if (!req || req->type != JSON_OBJECT) return 0;
    v = json_obj_get(req, "session_db_has_method");
    if (v && v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    return 0;
}

static ccd7fc1_resolved ccd7fc1_resolve(const json_t *req, const char *method) {
    ccd7fc1_resolved r;
    r.via = CCD7FC1_VIA_NONE;
    r.row = NULL;
    if (ccd7fc1_store_bound(req)) {
        r.row = ccd7fc1_find_verb(method);   /* absent -> KeyError upstream */
        r.via = CCD7FC1_VIA_STORE;
        return r;
    }
    if (ccd7fc1_db_has_method(req)) r.via = CCD7FC1_VIA_DB;
    return r;
}

static json_t *ccd7fc1_raised(const char *type) {
    json_t *out = json_object();
    json_set(out, "raised", json_string(type));
    return out;
}

json_t *w20_contextcom_7fc1af_resolve_verb(const json_t *req) {
    const char *method = ccd7fc1_str(req, "method");
    ccd7fc1_resolved r = ccd7fc1_resolve(req, method);
    json_t *out = json_object();
    if (r.via == CCD7FC1_VIA_STORE && !r.row) return ccd7fc1_raised("KeyError");
    if (r.via == CCD7FC1_VIA_STORE) {
        json_set(out, "via", json_string("store"));
        json_set(out, "repository", json_string(r.row->repository));
        json_set(out, "verb", json_string(r.row->verb));
        json_set(out, "callable", json_bool(1));
    } else if (r.via == CCD7FC1_VIA_DB) {
        json_set(out, "via", json_string("session_db"));
        json_set(out, "repository", json_null());
        json_set(out, "verb", json_string(method ? method : ""));
        json_set(out, "callable", json_bool(1));
    } else {
        json_set(out, "via", json_string("none"));
        json_set(out, "repository", json_null());
        json_set(out, "verb", json_string(method ? method : ""));
        json_set(out, "callable", json_bool(0));
    }
    return out;
}

/* ---- Python scalar coercion --------------------------------------------- */

static int ccd7fc1_truthy_str(const json_t *v) {
    return v && v->type == JSON_STRING && v->str_val && v->str_val[0];
}

/* `not session_id`: None or "" is unbound. */
static int ccd7fc1_session_unbound(const json_t *req) {
    json_t *v;
    if (!req || req->type != JSON_OBJECT) return 1;
    v = json_obj_get(req, "session_id");
    return !ccd7fc1_truthy_str(v);
}

/* type(x) is a scalar isinstance is int/float/str: bool counts (int subclass). */
static int ccd7fc1_is_scalar(const json_t *v) {
    if (!v) return 0;
    return v->type == JSON_BOOL || v->type == JSON_NUMBER || v->type == JSON_STRING;
}

/* int(str): Python strips surrounding whitespace, then takes an optional sign
 * and decimal digits with single underscores allowed between digits. */
static int ccd7fc1_parse_int_str(const char *s, long long *out) {
    const char *p = s;
    long long value = 0;
    int neg = 0, digits = 0, last_digit = 0;
    char *end = NULL;
    if (!s) return 0;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') p++;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    while (*p) {
        if (*p == '_') {
            if (!last_digit) return 0;      /* leading/double underscore: ValueError */
            last_digit = 0;
            p++;
            continue;
        }
        if (*p < '0' || *p > '9') break;
        if (value > (9223372036854775807LL - (*p - '0')) / 10) return 0;  /* overflow */
        value = value * 10 + (*p - '0');
        digits++;
        last_digit = 1;
        p++;
    }
    if (!digits || !last_digit) return 0;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') p++;
    if (*p) { (void)end; return 0; }
    *out = neg ? -value : value;
    return 1;
}

/* float(str): the same surrounding-whitespace tolerance as float(). */
static int ccd7fc1_parse_float_str(const char *s, double *out) {
    char buf[128];
    char *end = NULL;
    double d;
    size_t n, i = 0, j = 0;
    if (!s) return 0;
    n = strlen(s);
    if (n >= sizeof(buf)) return 0;
    while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r' ||
                     s[i] == '\f' || s[i] == '\v')) i++;
    for (; i < n; i++) {
        if (s[i] == '_') continue;
        buf[j++] = s[i];
    }
    buf[j] = '\0';
    if (!j) return 0;
    d = strtod(buf, &end);
    if (!end || *end) return 0;
    *out = d;
    return 1;
}

/* Python str() of an int-typed number is decimal; of a float, repr (5.0 -> "5.0").
 * The fixture only carries integral floats, whose str() is "<n>.0". */
static void ccd7fc1_number_text(const json_t *v, char *buf, size_t cap) {
    double d = v->num_val;
    if (isfinite(d) && floor(d) == d && fabs(d) < 9.007199254740992e15) {
        snprintf(buf, cap, "%.1f", d);
    } else {
        snprintf(buf, cap, "%.17g", d);
    }
}

/* max(default, coerced): numbers compare numerically; str vs number is a
 * TypeError, which the caller turns into (False, default). */
typedef struct {
    int ok;
    int is_text;
    double num;
    const char *text;      /* points into the request JSON */
    char textbuf[64];
} ccd7fc1_value;

static ccd7fc1_value ccd7fc1_coerce(const char *kind, const json_t *v) {
    ccd7fc1_value out;
    memset(&out, 0, sizeof(out));
    if (!kind || !strcmp(kind, "none")) {
        if (v->type == JSON_NUMBER) { out.num = v->num_val; out.ok = 1; }
        else if (v->type == JSON_BOOL) { out.num = v->bool_val ? 1.0 : 0.0; out.ok = 1; }
        else if (v->type == JSON_STRING) { out.text = v->str_val; out.is_text = 1; out.ok = 1; }
        return out;
    }
    if (!strcmp(kind, "int")) {
        long long n;
        if (v->type == JSON_BOOL) { out.num = v->bool_val ? 1.0 : 0.0; out.ok = 1; return out; }
        if (v->type == JSON_NUMBER) { out.num = trunc(v->num_val); out.ok = 1; return out; }
        if (v->type == JSON_STRING) {
            if (!ccd7fc1_parse_int_str(v->str_val, &n)) return out;   /* ValueError */
            out.num = (double)n;
            out.ok = 1;
            return out;
        }
        return out;
    }
    if (!strcmp(kind, "float")) {
        double n;
        if (v->type == JSON_BOOL) { out.num = v->bool_val ? 1.0 : 0.0; out.ok = 1; return out; }
        if (v->type == JSON_NUMBER) { out.num = v->num_val; out.ok = 1; return out; }
        if (v->type == JSON_STRING) {
            if (!ccd7fc1_parse_float_str(v->str_val, &n)) return out;  /* ValueError */
            out.num = n;
            out.ok = 1;
            return out;
        }
        return out;
    }
    if (!strcmp(kind, "str")) {
        if (v->type == JSON_STRING) { out.text = v->str_val; }
        else if (v->type == JSON_BOOL) { out.text = v->bool_val ? "True" : "False"; }
        else if (v->type == JSON_NUMBER) {
            ccd7fc1_number_text(v, out.textbuf, sizeof(out.textbuf));
            out.text = out.textbuf;
        } else {
            out.text = "";
        }
        out.is_text = 1;
        out.ok = 1;
        return out;
    }
    return out;
}

static json_t *ccd7fc1_read(const json_t *req) {
    const char *method = ccd7fc1_str(req, "method");
    const char *coerce = ccd7fc1_str(req, "coerce");
    const char *err = ccd7fc1_str(req, "error");
    json_t *stored = (req && req->type == JSON_OBJECT) ? json_obj_get(req, "stored") : NULL;
    double def = json_get_num(req, "default", 0.0);
    ccd7fc1_resolved resolved = ccd7fc1_resolve(req, method);
    json_t *out = json_object();

    /* `getter = _durable_verb(...)` runs before the unbound check and can
     * raise KeyError; the harness reports that instead of a fabricated answer. */
    if (resolved.via == CCD7FC1_VIA_STORE && !resolved.row) return ccd7fc1_raised("KeyError");
    if (ccd7fc1_session_unbound(req) || resolved.via == CCD7FC1_VIA_NONE || err) {
        json_set(out, "found", json_bool(0));
        json_set(out, "value", json_number(def));
        return out;
    }
    if (!ccd7fc1_is_scalar(stored)) {
        json_set(out, "found", json_bool(1));
        json_set(out, "value", json_null());
        return out;
    }
    {
        ccd7fc1_value value = ccd7fc1_coerce(coerce, stored);
        if (!value.ok) {
            json_set(out, "found", json_bool(0));
            json_set(out, "value", json_number(def));
            return out;
        }
        if (value.is_text) {
            /* max(default, "text") is a TypeError in Python 3. */
            json_set(out, "found", json_bool(0));
            json_set(out, "value", json_number(def));
            return out;
        }
        json_set(out, "found", json_bool(1));
        json_set(out, "value", json_number(value.num > def ? value.num : def));
        return out;
    }
}

json_t *w20_contextcom_7fc1af_durable_read(const json_t *req) {
    return ccd7fc1_read(req);
}

json_t *w20_contextcom_7fc1af_durable_write(const json_t *req) {
    const char *method = ccd7fc1_str(req, "method");
    const char *err = ccd7fc1_str(req, "error");
    ccd7fc1_resolved resolved = ccd7fc1_resolve(req, method);
    json_t *out = json_object();
    if (resolved.via == CCD7FC1_VIA_STORE && !resolved.row) return ccd7fc1_raised("KeyError");
    if (ccd7fc1_session_unbound(req) || resolved.via == CCD7FC1_VIA_NONE || err) {
        json_set(out, "ok", json_bool(0));
        return out;
    }
    json_set(out, "ok", json_bool(1));
    return out;
}

json_t *w20_contextcom_7fc1af_load_durable(const json_t *req) {
    json_t *read = ccd7fc1_read(req);
    json_t *out = json_object();
    json_t *found = json_obj_get(read, "found");
    json_t *value = json_obj_get(read, "value");
    int did = found && found->type == JSON_BOOL && found->bool_val;
    if (!found) {
        /* a raised KeyError rides through unchanged: the caller sees the crash */
        json_free(out);
        return read;
    }
    if (did) {
        json_t *def = json_obj_get(req, "default");
        if (!value || value->type == JSON_NULL) {
            json_set(out, "value", def ? json_copy(def) : json_number(0));
        } else {
            json_set(out, "value", json_copy(value));
        }
    } else {
        json_t *initial = (req && req->type == JSON_OBJECT) ? json_obj_get(req, "initial") : NULL;
        json_set(out, "value", initial ? json_copy(initial) : json_null());
    }
    json_free(read);
    return out;
}
