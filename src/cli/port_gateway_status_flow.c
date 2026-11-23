/*
 * port_gateway_status_flow.c — gateway/status.py liveness/status FLOW cores
 * and leaves. Decision sequences, call order, branch conditions, stale-record
 * detection, multiplex/host scopes and error paths; all OS/network/file I/O
 * via the universal seam (gsf_seams_t.call). See the header for the seam
 * table and the injected module-state object. No I/O is performed here.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "port_gateway_status_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

/* ── JSON / Python-value helpers ── */

static bool jtruth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
/* Python `x is not None` (a present, non-null node). */
static bool jpresent(const json_t *v) { return v && v->type != JSON_NULL; }
static bool is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
static const char *jstr(const json_t *v) {
    return (is_str(v) && v->str_val) ? v->str_val : NULL;
}

/* Python repr() escaping for an ASCII string embedded in an exception message. */
static char *gsf_repr(const char *in) {
    static char buf[512];
    size_t n = 0;
    if (!in) in = "";
    for (const char *p = in; *p && n + 2 < sizeof(buf) - 1; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '\n') { buf[n++] = '\\'; buf[n++] = 'n'; }
        else if (c == '\r') { buf[n++] = '\\'; buf[n++] = 'r'; }
        else if (c == '\t') { buf[n++] = '\\'; buf[n++] = 't'; }
        else if (c == '\\' || c == '\'') { buf[n++] = '\\'; buf[n++] = (char)c; }
        else buf[n++] = (char)c;
    }
    buf[n] = '\0';
    return buf;
}

static char *xsf(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf ? buf : strdup("");
}

/* Python type name of a parsed JSON value (integral doubles read as int). */
static const char *py_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return (v->num_val == (double)(long long)v->num_val) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default:          return "object";
    }
}

/* Python == across JSON values (bool is an int subclass; deep containers). */
static bool py_eq(const json_t *a, const json_t *b) {
    bool ap = jpresent(a), bp = jpresent(b);
    if (!ap || !bp) return ap == bp;
    if (is_bool(a) && is_bool(b)) return a->bool_val == b->bool_val;
    if ((is_bool(a) || is_num(a)) && (is_bool(b) || is_num(b))) {
        double av = is_bool(a) ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double bv = is_bool(b) ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return av == bv;
    }
    if (is_str(a) && is_str(b)) return strcmp(a->str_val, b->str_val) == 0;
    if (is_arr(a) && is_arr(b)) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++)
            if (!py_eq(a->c.items[i], b->c.items[i])) return false;
        return true;
    }
    if (is_obj(a) && is_obj(b)) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !py_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}

/* Shortest round-trip float repr (Python str(float)); integral -> int form. */
static char *py_float_str(double d) {
    if (d == (double)(long long)d) return xsf("%.0f", d);
    char buf[64];
    for (int prec = 15; prec <= 17; prec++) {
        snprintf(buf, sizeof(buf), "%.*g", prec, d);
        if (strtod(buf, NULL) == d) break;
    }
    return strdup(buf);
}

/* Python str() of a scalar JSON value (containers: repr syntax). */
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: return py_float_str(v->num_val);
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_ARRAY: {
            char *out = strdup("[");
            for (size_t i = 0; i < v->c.count; i++) {
                char *item = py_str(v->c.items[i]);
                char *next = xsf("%s%s%s", out, i ? ", " : "", item);
                free(out); free(item); out = next;
            }
            char *next = xsf("%s]", out); free(out); return next;
        }
        case JSON_OBJECT: {
            char *out = strdup("{");
            for (size_t i = 0; i < v->c.count; i++) {
                char *item = py_str(v->c.items[i]);
                char *next = xsf("%s%s'%s': %s", out, i ? ", " : "", v->c.keys[i], item);
                free(out); free(item); out = next;
            }
            char *next = xsf("%s}", out); free(out); return next;
        }
    }
    return strdup("");
}

/* Python repr() of a string: quote selection + the common escapes. */
static char *py_repr_str(const char *s) {
    bool has_sq = strchr(s, '\'') != NULL, has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t n = strlen(s);
    char *out = malloc(n * 4 + 3);
    if (!out) return strdup("''");
    size_t k = 0;
    out[k++] = q;
    for (size_t i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch == (unsigned char)q || ch == '\\') { out[k++] = '\\'; out[k++] = (char)ch; }
        else if (ch == '\n') { out[k++] = '\\'; out[k++] = 'n'; }
        else if (ch == '\r') { out[k++] = '\\'; out[k++] = 'r'; }
        else if (ch == '\t') { out[k++] = '\\'; out[k++] = 't'; }
        else if (ch < 0x20 || ch == 0x7f) { k += (size_t)sprintf(out + k, "\\x%02x", ch); }
        else out[k++] = (char)ch;
    }
    out[k++] = q; out[k] = '\0';
    return out;
}
/* repr() for the coerce-tag error: strings quoted, scalars str()==repr(). */
static char *py_repr(const json_t *v) {
    if (is_str(v)) return py_repr_str(jstr(v) ? jstr(v) : "");
    return py_str(v);
}

/* ── path helpers (Path.as_posix semantics for trace args) ── */

/* PathDouble normalization (oracle's scripted-resolve double): backslashes ->
 * slashes, collapse duplicate slashes, drop a trailing separator (len > 1).
 * NO "."-component removal and NO "" -> "." (the double keeps those). */
static char *pathdouble_norm(const char *s) {
    if (!s) return strdup("");
    size_t n = strlen(s);
    char *out = malloc(n + 2);
    if (!out) return strdup("");
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        char ch = (s[i] == '\\') ? '/' : s[i];
        if (ch == '/' && k > 0 && out[k - 1] == '/') continue;
        out[k++] = ch;
    }
    out[k] = '\0';
    while (k > 1 && out[k - 1] == '/') out[--k] = '\0';
    return out;
}

/* Normalize a path string the way Windows pathlib.Path + as_posix does:
 * backslashes -> slashes, collapse duplicate slashes, drop "." components
 * (pathlib keeps ".."), drop a trailing separator (root "/", drive root and a
 * bare UNC share root keep theirs), and the empty path is Path(".") -> ".".
 * A UNC share root "\\\\server\\share" stays "//server/share/" (two leading
 * slashes + trailing share slash); a single-root POSIX path "/x" stays "/x".
 * Caller frees. */
static char *path_norm(const char *s) {
    if (!s) return strdup(".");
    size_t slen = strlen(s);
    char *raw = (char *)malloc(slen + 1);
    if (!raw) return strdup(".");
    for (size_t i = 0; i < slen; i++) raw[i] = (s[i] == '\\') ? '/' : s[i];
    raw[slen] = '\0';
    bool unc = (slen >= 2 && raw[0] == '/' && raw[1] == '/');
    bool rooted = (slen >= 1 && raw[0] == '/');
    char *out = malloc(slen + 4);
    if (!out) { free(raw); return strdup("."); }
    size_t k = 0;
    if (unc) { out[k++] = '/'; out[k++] = '/'; }
    else if (rooted) out[k++] = '/';
    size_t i = unc ? 2 : (rooted ? 1 : 0);
    size_t comps = 0;
    while (i <= slen) {
        size_t j = i;
        while (j < slen && raw[j] != '/') j++;
        size_t len = j - i;
        if (len == 1 && raw[i] == '.') { /* drop "." component */ }
        else if (len > 0) {
            if (comps > 0) out[k++] = '/';
            memcpy(out + k, raw + i, len);
            k += len;
            comps++;
        }
        i = j + 1;
    }
    /* A bare UNC share root (server + share, nothing after it) keeps its
     * trailing separator, matching pathlib.Path(s).as_posix(). */
    if (unc && comps == 2 && out[k - 1] != '/') out[k++] = '/';
    out[k] = '\0';
    free(raw);
    if (k == 0) { out[0] = '.'; out[1] = '\0'; }
    return out;
}
/* An absolute Windows component resets Path(base) / name. Project the result
 * with as_posix(): drive roots keep C:/, UNC share roots keep //srv/shr/. */
static char *path_abs_posix(const char *name) {
    if (!name) return strdup("/");
    size_t n = strlen(name);
    char *s = (char *)malloc(n + 2);
    if (!s) return strdup("/");
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        char c = (name[i] == '\\') ? '/' : name[i];
        if (c == '/' && k > 0 && s[k - 1] == '/' && k != 1) continue;
        s[k++] = c;
    }
    while (k > 1 && s[k - 1] == '/') s[--k] = '\0';
    if (k == 2 && s[1] == ':' && ((s[0] >= 'A' && s[0] <= 'Z') || (s[0] >= 'a' && s[0] <= 'z'))) {
        s[k++] = '/'; s[k] = '\0';                /* drive root -> C:/ */
    } else if (k >= 2 && s[0] == '/' && s[1] == '/') {
        size_t i = 2;
        while (i < k && s[i] != '/') i++;          /* server */
        if (i < k) i++;
        while (i < k && s[i] != '/') i++;          /* share */
        if (i == k) { s[k++] = '/'; s[k] = '\0'; }  /* UNC share root */
    }
    return s;
}
/* Path(base) / name: an absolute name resets the base; the join re-normalizes
 * (a trailing separator on name, dot components, empty base -> dot). Caller frees. */
static char *path_join(const char *base, const char *name) {
    if (!name) name = "";
    /* POSIX absolute, drive-absolute, or UNC all reset the base. */
    if (name[0] == '/' || name[0] == '\\' ||
        (((name[0] >= 'A' && name[0] <= 'Z') || (name[0] >= 'a' && name[0] <= 'z'))
         && name[1] == ':'))
        return path_abs_posix(name);
    char *b = path_norm(base);
    char *raw = xsf("%s/%s", b, name);
    char *out = path_norm(raw);
    free(b);
    free(raw);
    return out;
}
/* os.path.normcase equality on normalized posix paths (Windows semantics). */
static bool normcase_eq(const char *a, const char *b) {
    if (!a || !b) return a == b;
    for (; *a && *b; a++, b++) {
        char ca = (*a >= 'A' && *a <= 'Z') ? (char)(*a + 32) : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? (char)(*b + 32) : *b;
        if (ca != cb) return false;
    }
    return *a == *b;
}

/* ── trace / seam / raise plumbing ── */

void gsf_trace_add(gsf_trace_t *t, json_t *ev) {
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
void gsf_trace_free(gsf_trace_t *t) {
    if (t->events) { json_free(t->events); t->events = NULL; }
}
json_t *gsf_call(const gsf_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}
const char *gsf_raise_class(const json_t *r) {
    if (is_obj(r)) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (is_str(c)) return c->str_val;
    }
    return NULL;
}
bool gsf_is_raise(const json_t *r) { return gsf_raise_class(r) != NULL; }

static json_t *raise_obj(const char *cls, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(cls));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}
static json_t *raise_typeerror(const char *msg) { return raise_obj("TypeError", msg); }
static json_t *raise_attr(const json_t *v, const char *attr) {
    char *m = xsf("'%s' object has no attribute '%s'", py_type_name(v), attr);
    json_t *r = raise_obj("AttributeError", m);
    free(m);
    return r;
}
static json_t *raise_keyerror(const char *key) {
    char *m = xsf("'%s'", key);
    json_t *r = raise_obj("KeyError", m);
    free(m);
    return r;
}
/* Propagate a seam/helper raise result (transfers ownership). */
#define GSF_PROP(r) do { if (gsf_is_raise(r)) return (r); } while (0)

/* `except Exception` catches everything but the BaseException-only kinds. */
static bool kind_base_only(const char *k) {
    return !strcmp(k, "SystemExit") || !strcmp(k, "KeyboardInterrupt")
        || !strcmp(k, "GeneratorExit") || !strcmp(k, "BaseException");
}
static bool kind_oserror(const char *k) {
    static const char *fam[] = {
        "OSError", "BlockingIOError", "ChildProcessError", "ConnectionError",
        "BrokenPipeError", "ConnectionAbortedError", "ConnectionRefusedError",
        "ConnectionResetError", "FileExistsError", "FileNotFoundError",
        "InterruptedError", "IsADirectoryError", "NotADirectoryError",
        "PermissionError", "ProcessLookupError", "TimeoutError",
        "UnsupportedOperation",
    };
    for (size_t i = 0; i < sizeof(fam) / sizeof(fam[0]); i++)
        if (!strcmp(k, fam[i])) return true;
    return false;
}

/* args builders — null-valued keys are dropped on BOTH sides (trace parity). */
static json_t *args0(void) { return json_object(); }
static void aset_j(json_t *a, const char *k, const json_t *v) {
    if (jpresent(v)) json_set(a, k, json_copy(v));
}
static void aset_s(json_t *a, const char *k, const char *v) {
    if (v) json_set(a, k, json_string(v));
}
static void aset_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void aset_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }

/* ── ISO-8601 parsing (datetime.fromisoformat subset the flows emit) ── */

static long long days_from_civil(long long y, unsigned m, unsigned d) {
    y -= m <= 2;
    long long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long long)doe - 719468;
}
static bool digits_n(const char *s, size_t n, int *out) {
    int v = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return false;
        v = v * 10 + (s[i] - '0');
    }
    *out = v;
    return true;
}
/* Parse a fromisoformat-compatible stamp to epoch seconds (+ micros fraction).
 * Accepts YYYY-MM-DD[<sep>HH[:MM[:SS]][.f{1,6}]][Z|z|±HH[:MM]|±HHMM].
 * Returns false on anything datetime.fromisoformat would reject. */
static bool parse_iso(const char *s, double *epoch, bool *has_tz) {
    *has_tz = false;
    if (!s) return false;
    size_t n = strlen(s);
    if (n < 10) return false;
    int y, mo, d;
    if (!digits_n(s, 4, &y) || s[4] != '-' || !digits_n(s + 5, 2, &mo)
        || s[7] != '-' || !digits_n(s + 8, 2, &d)) return false;
    if (mo < 1 || mo > 12 || d < 1) return false;
    static const int mdays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int maxd = mdays[mo - 1];
    if (mo == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) maxd = 29;
    if (d > maxd) return false;
    long long secs = days_from_civil(y, (unsigned)mo, (unsigned)d) * 86400LL;
    double frac = 0.0;
    size_t i = 10;
    if (i < n) {
        i++; /* separator: any single character (T, space, ...) */
        int h = 0, mi = 0, se = 0;
        if (i + 2 > n || !digits_n(s + i, 2, &h)) return false;
        i += 2;
        if (i < n && s[i] == ':') {
            if (i + 3 > n || !digits_n(s + i + 1, 2, &mi)) return false;
            i += 3;
            if (i < n && s[i] == ':') {
                if (i + 3 > n || !digits_n(s + i + 1, 2, &se)) return false;
                i += 3;
            }
        }
        if (h > 23 || mi > 59 || se > 59) return false;
        secs += h * 3600 + mi * 60 + se;
        if (i < n && s[i] == '.') {
            i++;
            size_t start = i;
            while (i < n && s[i] >= '0' && s[i] <= '9') i++;
            size_t fd = i - start;
            if (fd < 1 || fd > 6) return false;
            int fv = 0;
            digits_n(s + start, fd, &fv);
            while (fd < 6) { fv *= 10; fd++; }
            frac = (double)fv / 1000000.0;
        }
        if (i < n) {
            char c = s[i];
            if (c == 'Z' || c == 'z') {
                *has_tz = true;
                i++;
            } else if (c == '+' || c == '-') {
                int sign = (c == '-') ? -1 : 1;
                i++;
                int oh = 0, om = 0;
                if (i + 2 > n || !digits_n(s + i, 2, &oh)) return false;
                i += 2;
                if (i < n && s[i] == ':') {
                    if (i + 3 > n || !digits_n(s + i + 1, 2, &om)) return false;
                    i += 3;
                } else if (i + 2 <= n && s[i] >= '0' && s[i] <= '9') {
                    if (!digits_n(s + i, 2, &om)) return false;
                    i += 2;
                }
                if (oh > 23 || om > 59) return false;
                secs -= sign * (oh * 3600 + om * 60);
                *has_tz = true;
            } else {
                return false;
            }
        }
    }
    if (i != n) return false;
    *epoch = (double)secs + frac;
    return true;
}

/* ── _start_times_conflict (pure leaf) ── */
/* PoP: gsf_start_times_conflict @ gateway/status.py:_start_times_conflict */
json_t *gsf_start_times_conflict(const json_t *recorded, const json_t *current) {
    /* None not in (recorded_start, current_start) and current_start != recorded_start */
    bool both_known = jpresent(recorded) && jpresent(current);
    return json_bool(both_known && !py_eq(current, recorded));
}

/* ── _coerce_session_store (pure leaf) ── */
/* PoP: gsf_coerce_session_store @ gateway/status.py:_coerce_session_store */
json_t *gsf_coerce_session_store(const json_t *session_store) {
    /* state = str(session_store.get("status") or "") if isinstance(..., dict) else "" */
    char *state = strdup("");
    if (is_obj(session_store)) {
        const json_t *v = json_obj_get(session_store, "status");
        if (jtruth(v)) { free(state); state = py_str(v); }
    }
    const char *result = "unknown";
    if (!strcmp(state, "ok") || !strcmp(state, "unavailable") || !strcmp(state, "retrying"))
        result = state;
    json_t *o = json_object();
    json_set(o, "status", json_string(result));
    free(state);
    return o;
}

/* ── _pid_marker_names_self ── */
/* PoP: gsf_pid_marker_names_self @ gateway/status.py:_pid_marker_names_self */
json_t *gsf_pid_marker_names_self(double target_pid, const json_t *target_start_time,
                                  const gsf_seams_t *s) {
    json_t *a = args0();
    json_t *gp = gsf_call(s, "getpid", a);
    json_free(a);
    GSF_PROP(gp);
    json_t *tp = json_number(target_pid);
    bool match = py_eq(tp, gp);
    json_free(tp);
    json_free(gp);
    if (!match) return json_bool(false);
    a = args0(); aset_n(a, "pid", target_pid);
    json_t *our = gsf_call(s, "get_process_start_time", a);
    json_free(a);
    GSF_PROP(our);
    /* None in (target_start_time, our_start_time) or target_start_time == our_start_time */
    bool r = !jpresent(target_start_time) || !jpresent(our) || py_eq(target_start_time, our);
    json_free(our);
    return json_bool(r);
}

/* ── runtime_status_heartbeat_age_s ── */
/* PoP: gsf_runtime_status_heartbeat_age_s @ gateway/status.py:runtime_status_heartbeat_age_s */
json_t *gsf_runtime_status_heartbeat_age_s(const json_t *record, const gsf_seams_t *s) {
    if (!is_obj(record)) return NULL;
    const json_t *val = json_obj_get(record, "updated_at");
    json_t *a = args0(); aset_j(a, "value", val);
    json_t *norm = gsf_call(s, "normalize_updated_at", a);
    json_free(a);
    GSF_PROP(norm);
    if (!jtruth(norm)) { json_free(norm); return NULL; }
    /* datetime.now(UTC) is the LEFT operand: its seam event precedes the parse. */
    a = args0();
    json_t *nowr = gsf_call(s, "datetime_now", a);
    json_free(a);
    if (gsf_is_raise(nowr)) { json_free(norm); return nowr; }
    const char *us = jstr(norm);
    if (!us) {
        json_free(norm); json_free(nowr);
        return raise_typeerror("fromisoformat: argument must be str");
    }
    if (!is_num(nowr)) {
        json_free(norm); json_free(nowr);
        return raise_typeerror("argument must be int or float, not NoneType");
    }
    double epoch; bool has_tz;
    if (!parse_iso(us, &epoch, &has_tz)) {
        char *m = xsf("Invalid isoformat string: '%s'", gsf_repr(us));
        json_t *r = raise_obj("ValueError", m);
        free(m); json_free(norm); json_free(nowr);
        return r;
    }
    if (!has_tz) {
        json_free(norm); json_free(nowr);
        return raise_typeerror("can't subtract offset-naive and offset-aware datetimes");
    }
    double diff = nowr->num_val - epoch;
    long long age = (long long)diff; /* int() truncates toward zero */
    if (age < 0) age = 0;            /* max(0, ...) */
    json_free(norm); json_free(nowr);
    return json_number((double)age);
}

/* ── _emit_runtime_status_transition ── */
/* PoP: gsf_emit_runtime_status_transition @ gateway/status.py:_emit_runtime_status_transition */
json_t *gsf_emit_runtime_status_transition(const json_t *previous, const json_t *payload,
                                           const gsf_seams_t *s) {
    json_t *a = args0();
    aset_j(a, "previous_payload", previous);
    aset_j(a, "payload", payload);
    json_t *r = gsf_call(s, "emit_runtime_status_transition", a);
    json_free(a);
    if (gsf_is_raise(r)) {
        /* contextlib.suppress(Exception): BaseException-only kinds escape. */
        if (kind_base_only(gsf_raise_class(r))) return r;
        json_free(r);
    } else {
        json_free(r);
    }
    return NULL;
}

/* ── _merge_over_on_disk ── */
/* PoP: gsf_flow_merge_over_on_disk @ gateway/status.py:_merge_over_on_disk */
json_t *gsf_flow_merge_over_on_disk(const char *path, const json_t *payload,
                                    const gsf_seams_t *s) {
    json_t *a = args0(); aset_s(a, "path", path ? path : "");
    json_t *existing = gsf_call(s, "read_json_file", a);
    json_free(a);
    GSF_PROP(existing);
    json_t *result;
    if (is_obj(existing)) {
        if (!is_obj(payload)) {
            char *m = xsf("'%s' object is not a mapping", py_type_name(payload));
            json_t *r = raise_typeerror(m);
            free(m); json_free(existing);
            return r;
        }
        /* {**existing, **payload}: existing key order, payload wins on overlap. */
        result = json_copy(existing);
        for (size_t i = 0; i < payload->c.count; i++)
            json_set(result, payload->c.keys[i], json_copy(payload->c.items[i]));
    } else {
        result = payload ? json_copy(payload) : NULL;
    }
    json_free(existing);
    return result;
}

/* ── _probe_lock_file ── */
/* PoP: gsf_flow_probe_lock_file @ gateway/status.py:_probe_lock_file */
json_t *gsf_flow_probe_lock_file(const char *handle, const gsf_seams_t *s) {
    json_t *a = args0(); aset_s(a, "handle", handle);
    json_t *acq = gsf_call(s, "try_acquire_file_lock", a);
    json_free(a);
    json_t *pending = NULL;
    bool held = false;
    if (gsf_is_raise(acq)) {
        pending = acq;
    } else {
        held = !jtruth(acq);
        json_free(acq);
        if (!held) {
            a = args0(); aset_s(a, "handle", handle);
            json_t *rel = gsf_call(s, "release_file_lock", a);
            json_free(a);
            if (gsf_is_raise(rel)) pending = rel;
            else json_free(rel);
        }
    }
    /* finally: with contextlib.suppress(OSError): handle.close() */
    a = args0(); aset_s(a, "handle", handle);
    json_t *cl = gsf_call(s, "handle.close", a);
    json_free(a);
    if (gsf_is_raise(cl)) {
        if (kind_oserror(gsf_raise_class(cl))) {
            json_free(cl);
        } else {
            /* A raise in finally replaces the propagating exception. */
            if (pending) json_free(pending);
            return cl;
        }
    } else {
        json_free(cl);
    }
    if (pending) return pending;
    return json_bool(held);
}

/* ── _host_gateway_serves_home ── */
/* PoP: gsf_flow_host_gateway_serves_home @ gateway/status.py:_host_gateway_serves_home */
json_t *gsf_flow_host_gateway_serves_home(double pid, const char *profile_home,
                                          const gsf_seams_t *s) {
    json_t *a = args0();
    json_t *owner = gsf_call(s, "host_gateway", a);
    json_free(a);
    if (gsf_is_raise(owner)) {
        /* try: import + host_gateway() — except Exception: return False */
        if (kind_base_only(gsf_raise_class(owner))) return owner;
        json_free(owner);
        return json_bool(false);
    }
    if (!owner) return json_bool(false); /* owner is not None */
    if (!is_obj(owner)) {
        json_t *r = raise_attr(owner, "pid");
        json_free(owner);
        return r;
    }
    const json_t *opid = json_obj_get(owner, "pid");
    json_t *pnode = json_number(pid);
    bool pid_match = py_eq(opid, pnode);
    json_free(pnode);
    if (!pid_match) { json_free(owner); return json_bool(false); }
    char *phn = path_norm(profile_home);
    a = args0(); aset_s(a, "profile_home", phn);
    free(phn);
    json_t *name = gsf_call(s, "host_profile_name_for_home", a);
    json_free(a);
    if (gsf_is_raise(name)) { json_free(owner); return name; }
    a = args0(); aset_j(a, "name", name);
    json_t *serves = gsf_call(s, "owner.serves", a);
    json_free(a);
    json_free(name);
    if (gsf_is_raise(serves)) { json_free(owner); return serves; }
    bool r = jtruth(serves);
    json_free(serves);
    json_free(owner);
    return json_bool(r);
}

/* ── _live_pid_from_record ── */
/* PoP: gsf_flow_live_pid_from_record @ gateway/status.py:_live_pid_from_record */
json_t *gsf_flow_live_pid_from_record(const json_t *record, const gsf_seams_t *s) {
    json_t *a = args0();
    aset_j(a, "record", record);
    aset_s(a, "key", "pid");
    json_t *pid = gsf_call(s, "pid_from_record", a);
    json_free(a);
    GSF_PROP(pid);
    if (!jpresent(pid)) { json_free(pid); return NULL; }
    a = args0(); aset_j(a, "pid", pid);
    json_t *alive = gsf_call(s, "pid_exists", a);
    json_free(a);
    if (gsf_is_raise(alive)) { json_free(pid); return alive; }
    bool live = jtruth(alive);
    json_free(alive);
    if (!live) { json_free(pid); return NULL; }
    /* record.get("start_time") — a non-dict record raises AttributeError. */
    if (!is_obj(record)) {
        json_t *r = raise_attr(record, "get");
        json_free(pid);
        return r;
    }
    const json_t *st = json_obj_get(record, "start_time");
    a = args0(); aset_j(a, "pid", pid);
    json_t *cur = gsf_call(s, "get_process_start_time", a);
    json_free(a);
    if (gsf_is_raise(cur)) { json_free(pid); return cur; }
    json_t *conf = gsf_start_times_conflict(st, cur);
    bool c = conf->bool_val;
    json_free(conf);
    json_free(cur);
    if (c) { json_free(pid); return NULL; }
    return pid;
}

/* list(v): array copy, string -> code points, dict -> keys, else TypeError. */
static json_t *py_list(const json_t *v) {
    if (is_arr(v)) return json_copy(v);
    if (is_str(v)) {
        json_t *a = json_array();
        const unsigned char *s = (const unsigned char *)v->str_val;
        size_t n = strlen(v->str_val), i = 0;
        while (i < n) {
            size_t len = 1;
            unsigned char c = s[i];
            if (c >= 0xF0) len = 4;
            else if (c >= 0xE0) len = 3;
            else if (c >= 0xC0) len = 2;
            if (i + len > n) len = n - i;
            char *ch = malloc(len + 1);
            if (ch) { memcpy(ch, s + i, len); ch[len] = '\0'; json_append(a, json_string(ch)); free(ch); }
            i += len;
        }
        return a;
    }
    if (is_obj(v)) {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++) json_append(a, json_string(v->c.keys[i]));
        return a;
    }
    char *m = xsf("'%s' object is not iterable", py_type_name(v));
    json_t *r = raise_typeerror(m);
    free(m);
    return r;
}

/* except Exception: true when the raise must propagate (BaseException-only). */
static bool swallow_exception(json_t *r) {
    if (kind_base_only(gsf_raise_class(r))) return true;
    json_free(r);
    return false;
}

/* ── _read_live_pid_marker ── */
/* PoP: gsf_flow_read_live_pid_marker @ gateway/status.py:_read_live_pid_marker */
json_t *gsf_flow_read_live_pid_marker(const char *path, double ttl_s, const gsf_seams_t *s) {
    char *pn = path_norm(path);
    json_t *a = args0(); aset_s(a, "path", pn);
    json_t *record = gsf_call(s, "read_json_file", a);
    json_free(a);
    if (gsf_is_raise(record)) { free(pn); return record; }
    if (!jtruth(record)) { json_free(record); free(pn); return NULL; }
    a = args0(); aset_j(a, "record", record); aset_s(a, "key", "target_pid");
    json_t *tp = gsf_call(s, "pid_from_record", a);
    json_free(a);
    if (gsf_is_raise(tp)) { json_free(record); free(pn); return tp; }
    /* target_pid is None or _marker_is_stale(record.get("written_at") or "", ttl_s)
     * — short-circuit: no stale probe (and no .get) when target_pid is None. */
    bool is_stale = false;
    if (jpresent(tp)) {
        if (!is_obj(record)) {
            json_t *r = raise_attr(record, "get");
            json_free(tp); json_free(record); free(pn);
            return r;
        }
        const json_t *wa = json_obj_get(record, "written_at");
        json_t *wa_arg = jtruth(wa) ? json_copy(wa) : json_string("");
        a = args0(); aset_j(a, "written_at", wa_arg); aset_n(a, "ttl_s", ttl_s);
        json_free(wa_arg);
        json_t *stale = gsf_call(s, "marker_is_stale", a);
        json_free(a);
        if (gsf_is_raise(stale)) { json_free(tp); json_free(record); free(pn); return stale; }
        is_stale = jtruth(stale);
        json_free(stale);
    }
    if (!jpresent(tp) || is_stale) {
        if (!is_obj(record)) {
            json_t *r = raise_attr(record, "get");
            json_free(tp); json_free(record); free(pn);
            return r;
        }
        const json_t *accepted = json_obj_get(record, "accepted");
        bool accepted_true = is_bool(accepted) && accepted->bool_val;
        bool do_unlink = true;
        if (accepted_true) {
            /* record.get("accepted") is not True or not _update_pause_on_disk() */
            a = args0();
            json_t *pause = gsf_call(s, "update_pause_on_disk", a);
            json_free(a);
            if (gsf_is_raise(pause)) { json_free(tp); json_free(record); free(pn); return pause; }
            do_unlink = !jtruth(pause);
            json_free(pause);
        }
        if (do_unlink) {
            a = args0(); aset_s(a, "path", pn);
            json_t *ul = gsf_call(s, "unlink_quietly", a);
            json_free(a);
            if (gsf_is_raise(ul)) { json_free(tp); json_free(record); free(pn); return ul; }
            json_free(ul);
        }
        json_free(tp); json_free(record); free(pn);
        return NULL;
    }
    json_t *out = json_array();
    json_append(out, json_copy(record));
    json_append(out, json_copy(tp));
    const json_t *tst = json_obj_get(record, "target_start_time");
    json_append(out, tst ? json_copy(tst) : json_null());
    json_free(tp); json_free(record); free(pn);
    return out;
}

/* ── _terminate_verified_owner ── */
/* PoP: gsf_flow_terminate_verified_owner @ gateway/status.py:_terminate_verified_owner */
json_t *gsf_flow_terminate_verified_owner(double owner_pid, double owner_start_time,
                                          double graceful_attempts, double force_attempts,
                                          const gsf_seams_t *s) {
    json_t *a = args0();
    aset_n(a, "owner_pid", owner_pid);
    aset_n(a, "owner_start_time", owner_start_time);
    json_t *state = gsf_call(s, "scoped_lock_owner_state", a);
    json_free(a);
    GSF_PROP(state);
    const char *st = jstr(state);
    if (st && !strcmp(st, "exited")) { json_free(state); return json_number(owner_pid); }
    if (!st || strcmp(st, "same") != 0) { json_free(state); return NULL; }
    json_free(state);
    for (int step = 0; step < 2; step++) {
        double attempts = (step == 0) ? graceful_attempts : force_attempts;
        double delay = (step == 0) ? 0.5 : 0.25;
        a = args0();
        aset_n(a, "pid", owner_pid);
        aset_b(a, "force", step == 1);
        if (step == 1) aset_n(a, "expected_start_time", owner_start_time);
        json_t *term = gsf_call(s, "terminate_pid", a);
        json_free(a);
        if (gsf_is_raise(term)) {
            const char *k = gsf_raise_class(term);
            if (!strcmp(k, "ProcessLookupError")) { json_free(term); return json_number(owner_pid); }
            if (kind_oserror(k)) { json_free(term); return NULL; }
            return term;
        }
        json_free(term);
        a = args0();
        aset_n(a, "owner_pid", owner_pid);
        aset_n(a, "owner_start_time", owner_start_time);
        aset_n(a, "attempts", attempts);
        aset_n(a, "delay", delay);
        json_t *w = gsf_call(s, "wait_for_scoped_lock_owner_exit", a);
        json_free(a);
        GSF_PROP(w);
        if (!is_arr(w) || w->c.count != 2) {
            json_t *r;
            if (!w) {
                r = raise_typeerror("cannot unpack non-iterable NoneType object");
            } else if (!is_arr(w)) {
                char *m = xsf("cannot unpack non-iterable %s object", py_type_name(w));
                r = raise_typeerror(m);
                free(m);
            } else {
                char *m = xsf("%s values to unpack (expected 2, got %zu)",
                              w->c.count < 2 ? "not enough" : "too many", w->c.count);
                r = raise_obj("ValueError", m);
                free(m);
            }
            json_free(w);
            return r;
        }
        bool exited = jtruth(w->c.items[0]);
        bool safe = jtruth(w->c.items[1]);
        json_free(w);
        if (exited) return json_number(owner_pid);
        if (!safe) return NULL;
    }
    return NULL;
}

/* ── live_gateway_pid_for_home ── */
/* PoP: gsf_flow_live_gateway_pid_for_home @ gateway/status.py:live_gateway_pid_for_home */
json_t *gsf_flow_live_gateway_pid_for_home(const char *home, const gsf_seams_t *s) {
    char *hn = path_norm(home);
    char *pid_path = path_join(hn, "gateway.pid");
    json_t *a = args0();
    aset_s(a, "pid_path", pid_path);
    aset_b(a, "cleanup_stale", false);
    free(pid_path);
    json_t *pid = gsf_call(s, "get_running_pid_cached", a);
    json_free(a);
    if (gsf_is_raise(pid)) { free(hn); return pid; }
    if (jpresent(pid)) { free(hn); return pid; } /* pid is not None (0 counts) */
    json_free(pid);
    char *state_path = path_join(hn, "gateway_state.json");
    a = args0(); aset_s(a, "path", state_path);
    free(state_path);
    json_t *rt = gsf_call(s, "read_runtime_status", a);
    json_free(a);
    if (gsf_is_raise(rt)) { free(hn); return rt; }
    a = args0();
    aset_j(a, "runtime", rt);
    aset_s(a, "expected_home", hn);
    json_t *r = gsf_call(s, "get_runtime_status_running_pid", a);
    json_free(a);
    json_free(rt);
    free(hn);
    return r;
}

/* ── profile_name_for_home ── */
/* PoP: gsf_profile_name_for_home @ gateway/status.py:profile_name_for_home */
json_t *gsf_profile_name_for_home(const char *profile_home, const gsf_seams_t *s) {
    char *hn = pathdouble_norm(profile_home);
    json_t *a = args0(); aset_s(a, "profile_home", hn);
    json_t *named = gsf_call(s, "profile_name_for_home_priv", a);
    json_free(a);
    if (gsf_is_raise(named)) { free(hn); return named; } /* outside the try */
    if (jtruth(named)) { free(hn); return named; }
    json_free(named);
    /* try: home.resolve() == Path(get_default_hermes_root()).resolve() */
    a = args0(); aset_s(a, "path", hn);
    json_t *r1 = gsf_call(s, "path_resolve", a);
    json_free(a);
    if (gsf_is_raise(r1)) {
        free(hn);
        if (swallow_exception(r1)) return r1;
        return NULL;
    }
    char *r1raw = jstr(r1) ? strdup(jstr(r1)) : py_str(r1);
    char *r1s = pathdouble_norm(r1raw); /* resolve() returns PathDouble(ret) */
    free(r1raw);
    json_free(r1);
    a = args0();
    json_t *root = gsf_call(s, "get_default_hermes_root", a);
    json_free(a);
    if (gsf_is_raise(root)) {
        free(hn); free(r1s);
        if (swallow_exception(root)) return root;
        return NULL;
    }
    if (!root) { /* Path(None) -> TypeError, caught */
        free(hn); free(r1s);
        return NULL;
    }
    char *roots = jstr(root) ? strdup(jstr(root)) : py_str(root);
    json_free(root);
    /* The oracle's get_default_hermes_root stub returns REAL Path(ret)
     * ("" -> "."), then live wraps it in PathDouble: normalize with real
     * pathlib semantics before the scripted resolve() call. */
    char *rootn = path_norm(roots);
    free(roots);
    a = args0(); aset_s(a, "path", rootn);
    json_t *r2 = gsf_call(s, "path_resolve", a);
    json_free(a);
    free(rootn);
    if (gsf_is_raise(r2)) {
        free(hn); free(r1s);
        if (swallow_exception(r2)) return r2;
        return NULL;
    }
    char *r2raw = jstr(r2) ? strdup(jstr(r2)) : py_str(r2);
    char *r2s = pathdouble_norm(r2raw);
    free(r2raw);
    json_free(r2);
    bool eq = normcase_eq(r1s, r2s);
    free(hn); free(r1s); free(r2s);
    return eq ? json_string("default") : NULL;
}

/* ── profile_platforms_from_multiplexer ── */
/* PoP: gsf_profile_platforms_from_multiplexer @ gateway/status.py:profile_platforms_from_multiplexer */
json_t *gsf_profile_platforms_from_multiplexer(const json_t *runtime, const char *profile,
                                               const gsf_seams_t *s) {
    json_t *empty = NULL;
    const json_t *rt;
    if (jtruth(runtime)) {
        if (!is_obj(runtime)) return raise_attr(runtime, "get");
        rt = runtime;
    } else {
        empty = json_object();
        rt = empty;
    }
    const json_t *plats = json_obj_get(rt, "platforms");
    if (!is_obj(plats)) { json_free(empty); return json_object(); }
    char *prefix = xsf("%s:", profile ? profile : "");
    size_t plen = strlen(prefix);
    json_t *own = json_object();
    for (size_t i = 0; i < plats->c.count; i++) {
        const char *k = plats->c.keys[i];
        if (!strncmp(k, prefix, plen) && is_obj(plats->c.items[i]))
            json_set(own, k + plen, json_copy(plats->c.items[i]));
    }
    if (profile && !strcmp(profile, "default")) {
        /* The flat keys ARE the default's own adapters; they win on overlap. */
        for (size_t i = 0; i < plats->c.count; i++) {
            const char *k = plats->c.keys[i];
            if (!strchr(k, ':') && is_obj(plats->c.items[i]))
                json_set(own, k, json_copy(plats->c.items[i]));
        }
    }
    free(prefix);
    json_t *a = args0();
    aset_j(a, "runtime", runtime);
    aset_s(a, "profile", profile);
    json_t *mirror = gsf_call(s, "shared_listener_mirror_platforms", a);
    json_free(a);
    if (gsf_is_raise(mirror)) { json_free(own); json_free(empty); return mirror; }
    if (!is_obj(mirror)) {
        char *m = xsf("'%s' object is not a mapping", py_type_name(mirror));
        json_t *r = raise_typeerror(m);
        free(m); json_free(own); json_free(empty); json_free(mirror);
        return r;
    }
    /* {**mirror, **own}: mirror key order first, own wins on overlap. */
    json_t *result = json_copy(mirror);
    for (size_t i = 0; i < own->c.count; i++)
        json_set(result, own->c.keys[i], json_copy(own->c.items[i]));
    json_free(mirror); json_free(own); json_free(empty);
    return result;
}

/* ── _apply_set_fields ── */
/* PoP: gsf_apply_set_fields @ gateway/status.py:_apply_set_fields */
json_t *gsf_apply_set_fields(json_t *target, const json_t *fields, const gsf_seams_t *s) {
    if (!is_arr(fields) || !is_obj(target)) return NULL;
    /* The oracle's make_coerce runs for EVERY field during preparation,
     * before any assignment: an unknown tag raises ValueError and the target
     * stays untouched (even for fields whose value is _UNSET). */
    for (size_t i = 0; i < fields->c.count; i++) {
        const json_t *f = fields->c.items[i];
        if (!is_obj(f)) continue;
        const json_t *ct = json_obj_get(f, "coerce");
        if (!ct || ct->type == JSON_NULL) continue;
        const char *tag = jstr(ct);
        bool known = tag && (!strcmp(tag, "bool") || !strcmp(tag, "parse_active_agents")
            || !strcmp(tag, "active_work") || !strcmp(tag, "served_profiles")
            || !strcmp(tag, "parked_profiles") || !strcmp(tag, "multiplex_reason")
            || !strcmp(tag, "session_store"));
        if (!known) {
            char *rv = py_repr(ct);
            char *m = xsf("unknown coerce tag: %s", rv);
            json_t *r = raise_obj("ValueError", m);
            free(rv); free(m);
            return r;
        }
    }
    for (size_t i = 0; i < fields->c.count; i++) {
        const json_t *f = fields->c.items[i];
        if (!is_obj(f)) continue;
        const char *key = json_get_str(f, "key", NULL);
        if (!key) continue;
        if (!json_has(f, "value")) continue; /* value is _UNSET */
        const json_t *v = json_obj_get(f, "value");
        const char *coerce = json_get_str(f, "coerce", NULL);
        json_t *assigned = NULL;
        if (!coerce) {
            assigned = v ? json_copy(v) : json_null();
        } else if (!strcmp(coerce, "bool")) {
            assigned = json_bool(jtruth(v));
        } else if (!strcmp(coerce, "parse_active_agents")) {
            json_t *a = args0(); aset_j(a, "raw", v);
            assigned = gsf_call(s, "parse_active_agents", a);
            json_free(a);
            GSF_PROP(assigned);
        } else if (!strcmp(coerce, "active_work")) {
            /* lambda v: list(v) if v else None */
            assigned = jtruth(v) ? py_list(v) : json_null();
            if (gsf_is_raise(assigned)) return assigned;
        } else if (!strcmp(coerce, "served_profiles")) {
            /* lambda v: list(v or []) */
            assigned = jtruth(v) ? py_list(v) : json_array();
            if (gsf_is_raise(assigned)) return assigned;
        } else if (!strcmp(coerce, "parked_profiles")) {
            /* lambda v: {str(k): str(r) for k, r in dict(v or {}).items()}
             * (fixture domain: object or falsy; scalar truthy -> TypeError) */
            const json_t *src = jtruth(v) ? v : NULL;
            if (src && !is_obj(src)) {
                char *m = xsf("'%s' object is not iterable", py_type_name(src));
                assigned = raise_typeerror(m);
                free(m);
                return assigned;
            }
            assigned = json_object();
            if (src) {
                for (size_t k = 0; k < src->c.count; k++) {
                    char *sv = py_str(src->c.items[k]);
                    json_set(assigned, src->c.keys[k], json_string(sv));
                    free(sv);
                }
            }
        } else if (!strcmp(coerce, "multiplex_reason")) {
            /* lambda v: str(v) if v else None */
            if (jtruth(v)) {
                char *sv = py_str(v);
                assigned = json_string(sv);
                free(sv);
            } else {
                assigned = json_null();
            }
        } else if (!strcmp(coerce, "session_store")) {
            assigned = gsf_coerce_session_store(v);
        } else {
            assigned = v ? json_copy(v) : json_null();
        }
        json_set(target, key, assigned);
    }
    return NULL;
}

/* dict.update(src): object overlay; array of pairs; else TypeError. */
static json_t *dict_update(json_t *target, const json_t *src) {
    if (is_obj(src)) {
        for (size_t i = 0; i < src->c.count; i++)
            json_set(target, src->c.keys[i], json_copy(src->c.items[i]));
        return NULL;
    }
    if (is_arr(src)) {
        for (size_t i = 0; i < src->c.count; i++) {
            const json_t *e = src->c.items[i];
            const json_t *k = NULL, *v = NULL;
            if (is_arr(e) && e->c.count == 2) { k = e->c.items[0]; v = e->c.items[1]; }
            else if (is_str(e) && e->str_val && strlen(e->str_val) == 2) {
                char kb[2] = { e->str_val[0], '\0' };
                char vb[2] = { e->str_val[1], '\0' };
                json_set(target, kb, json_string(vb));
                continue;
            }
            if (!k || !is_str(k)) {
                char *m = xsf("cannot convert dictionary update sequence element #%zu to a sequence", i);
                json_t *r = raise_typeerror(m);
                free(m);
                return r;
            }
            json_set(target, k->str_val, json_copy(v));
        }
        return NULL;
    }
    char *m = xsf("'%s' object is not iterable", py_type_name(src));
    json_t *r = raise_typeerror(m);
    free(m);
    return r;
}

/* One (key, value, coerce) triple for gsf_apply_set_fields; a missing source
 * key stays _UNSET (no "value" member). */
static void add_set_field(json_t *arr, const json_t *src, const char *key, const char *coerce) {
    json_t *f = json_object();
    json_set(f, "key", json_string(key));
    if (json_has(src, key)) json_set(f, "value", json_copy(json_obj_get(src, key)));
    if (coerce) json_set(f, "coerce", json_string(coerce));
    json_append(arr, f);
}
/* Like add_set_field but the target key differs from the source field name
 * (live: ("state", platform_state), ("metrics", platform_metrics)). */
static void add_set_field_as(json_t *arr, const json_t *src, const char *srckey,
                             const char *dstkey, const char *coerce) {
    json_t *f = json_object();
    json_set(f, "key", json_string(dstkey));
    if (json_has(src, srckey)) json_set(f, "value", json_copy(json_obj_get(src, srckey)));
    if (coerce) json_set(f, "coerce", json_string(coerce));
    json_append(arr, f);
}
/* Like add_set_field but with an explicit default when the source key is
 * _UNSET (the platform_state == "connected" branch). */
static void add_set_field_def(json_t *arr, const json_t *src, const char *key,
                              const char *coerce, json_t *defval) {
    json_t *f = json_object();
    json_set(f, "key", json_string(key));
    if (json_has(src, key)) json_set(f, "value", json_copy(json_obj_get(src, key)));
    else json_set(f, "value", defval);
    if (coerce) json_set(f, "coerce", json_string(coerce));
    json_append(arr, f);
}

/* ── _prepare_runtime_status_update ──
 * Error paths leak the small per-case allocations (one-shot harness process);
 * every normal path frees everything it owns. */
/* PoP: gsf_flow_prepare_runtime_status @ gateway/status.py:_prepare_runtime_status_update */
json_t *gsf_flow_prepare_runtime_status(json_t *st, const json_t *fields, const gsf_seams_t *s) {
    json_t *a = args0();
    json_t *pathr = gsf_call(s, "get_runtime_status_path", a);
    json_free(a);
    GSF_PROP(pathr);
    const char *path = jstr(pathr);
    if (!path) {
        json_free(pathr);
        return raise_typeerror("expected str, bytes or os.PathLike object, not NoneType");
    }
    char *pn = path_norm(path);
    json_free(pathr);
    if (!is_obj(fields)) fields = NULL;
    /* The oracle seeds _runtime_status_state_path = Path(sp) if sp else None:
     * normalize the seeded spelling up front (st_out reads it back as_posix). */
    const json_t *sp = json_obj_get(st, "state_path");
    char *spn = NULL;
    if (is_str(sp) && jstr(sp)[0]) {
        spn = path_norm(jstr(sp));
        json_set(st, "state_path", json_string(spn));
    } else {
        json_set(st, "state_path", json_null());
    }
    const json_t *ss = json_obj_get(st, "state");
    /* if reload_existing or _runtime_status_state_path != path or _runtime_status_state is None */
    bool reload = (fields && jtruth(json_obj_get(fields, "reload_existing")))
        || !spn || !normcase_eq(spn, pn) || !jpresent(ss);
    if (reload) {
        json_set(st, "state_path", json_string(pn));
        json_t *existing = NULL;
        bool load_existing = !fields || !json_has(fields, "load_existing")
            || jtruth(json_obj_get(fields, "load_existing"));
        if (load_existing) {
            a = args0(); aset_s(a, "path", pn);
            existing = gsf_call(s, "read_json_file", a);
            json_free(a);
            GSF_PROP(existing);
        }
        if (!jtruth(existing)) {
            json_free(existing);
            a = args0();
            existing = gsf_call(s, "build_runtime_status_record", a);
            json_free(a);
            GSF_PROP(existing);
        }
        /* Live ASSIGNS the or-chain result even when it is None; a NULL
         * pointer here would drop the key and keep the stale seeded state. */
        json_set(st, "state", existing ? existing : json_null());
    }
    const json_t *previous = json_obj_get(st, "state");
    json_t *payload = previous ? json_copy(previous) : NULL;
    /* Live order: current_record = _build_pid_record() BEFORE payload.setdefault
     * (a None snapshot raises AttributeError only after the pid seam ran). */
    a = args0();
    json_t *cur = gsf_call(s, "build_pid_record", a);
    json_free(a);
    GSF_PROP(cur);
    if (!is_obj(payload)) {
        /* payload.setdefault on a non-dict snapshot */
        json_t *r = raise_attr(payload, "setdefault");
        json_free(payload);
        json_free(cur);
        free(pn); free(spn);
        return r;
    }
    /* payload.setdefault("platforms", {}); non-dict -> {} */
    json_t *plats = json_obj_get(payload, "platforms");
    if (!is_obj(plats)) {
        plats = json_object();
        json_set(payload, "platforms", plats);
    }
    const json_t *drop = fields ? json_obj_get(fields, "drop_profile_platforms") : NULL;
    bool clear = fields && jtruth(json_obj_get(fields, "clear_profile_platforms"));
    if (clear || jtruth(drop)) {
        const char *drops = jstr(drop);
        char *drop_prefix = drops ? xsf("%s:", drops) : NULL;
        size_t dplen = drop_prefix ? strlen(drop_prefix) : 0;
        const json_t *keep = fields ? json_obj_get(fields, "keep_platforms") : NULL;
        json_t *filtered = json_object();
        for (size_t i = 0; i < plats->c.count; i++) {
            const char *k = plats->c.keys[i];
            bool in_keep = false;
            if (is_arr(keep)) {
                for (size_t j = 0; j < keep->c.count; j++) {
                    const json_t *kv = keep->c.items[j];
                    if (is_str(kv) && !strcmp(kv->str_val, k)) { in_keep = true; break; }
                }
            }
            bool keep_it = !strchr(k, ':')
                || (drop_prefix && strncmp(k, drop_prefix, dplen) != 0)
                || in_keep;
            if (keep_it) json_set(filtered, k, json_copy(plats->c.items[i]));
        }
        json_set(payload, "platforms", filtered);
        plats = filtered;
        free(drop_prefix);
    }
    /* payload.update({key: current_record[key] for key in (...)}) — the
     * subscript happens HERE (after setdefault/platforms/clear), so a non-dict
     * pid record raises only now, matching live's evaluation order. */
    if (!is_obj(cur)) {
        char *m = xsf("'%s' object is not subscriptable", py_type_name(cur));
        json_t *r = raise_typeerror(m);
        free(m); json_free(cur); json_free(payload); free(pn); free(spn);
        return r;
    }
    static const char *idkeys[] = { "kind", "pid", "argv", "start_time" };
    for (size_t j = 0; j < 4; j++) {
        const json_t *cv = json_obj_get(cur, idkeys[j]);
        if (!cv) {
            json_t *r = raise_keyerror(idkeys[j]);
            json_free(cur); json_free(payload); free(pn);
            return r;
        }
        json_set(payload, idkeys[j], json_copy(cv));
    }
    a = args0();
    json_t *ts = gsf_call(s, "utc_now_iso", a);
    json_free(a);
    GSF_PROP(ts);
    json_set(payload, "updated_at", ts);
    a = args0();
    json_t *ident = gsf_call(s, "get_code_identity_fields", a);
    json_free(a);
    GSF_PROP(ident);
    json_t *up = dict_update(payload, ident);
    json_free(ident);
    GSF_PROP(up);
    /* _apply_set_fields(payload, (...)) — live tuple order. */
    json_t *sf = json_array();
    add_set_field(sf, fields, "gateway_state", NULL);
    add_set_field(sf, fields, "exit_reason", NULL);
    add_set_field(sf, fields, "restart_requested", "bool");
    add_set_field(sf, fields, "active_agents", "parse_active_agents");
    add_set_field(sf, fields, "active_work", "active_work");
    add_set_field(sf, fields, "served_profiles", "served_profiles");
    add_set_field(sf, fields, "parked_profiles", "parked_profiles");
    add_set_field(sf, fields, "multiplex_standalone_reason", "multiplex_reason");
    add_set_field(sf, fields, "session_store", "session_store");
    json_t *ar = gsf_apply_set_fields(payload, sf, s);
    json_free(sf);
    GSF_PROP(ar);
    /* if platform is not _UNSET: ... */
    if (fields && json_has(fields, "platform")) {
        const json_t *pv = json_obj_get(fields, "platform");
        if (!is_str(pv)) {
            /* fixture domain: string platform keys only */
            char *m = xsf("unhashable type: '%s'", py_type_name(pv));
            json_t *r = raise_typeerror(m);
            free(m); json_free(cur); json_free(payload); free(pn);
            return r;
        }
        const char *platform = pv->str_val;
        const json_t *pp_src = json_obj_get(plats, platform);
        json_t *pp = is_obj(pp_src) ? json_copy(pp_src) : json_object();
        const json_t *pstate = json_obj_get(fields, "platform_state");
        bool connected = is_str(pstate) && !strcmp(pstate->str_val, "connected");
        json_t *pf = json_array();
        add_set_field_as(pf, fields, "platform_state", "state", NULL);
        add_set_field(pf, fields, "error_code", NULL);
        add_set_field(pf, fields, "error_message", NULL);
        if (connected) {
            add_set_field_def(pf, fields, "needs_attention", "bool", json_bool(false));
            add_set_field_def(pf, fields, "retrying_since", NULL, json_null());
        } else {
            add_set_field(pf, fields, "needs_attention", "bool");
            add_set_field(pf, fields, "retrying_since", NULL);
        }
        add_set_field_as(pf, fields, "platform_metrics", "metrics", NULL);
        add_set_field(pf, fields, "ingress_url", NULL);
        add_set_field(pf, fields, "listener_base", NULL);
        json_t *ar2 = gsf_apply_set_fields(pp, pf, s);
        json_free(pf);
        if (gsf_is_raise(ar2)) { json_free(pp); json_free(cur); json_free(payload); free(pn); return ar2; }
        a = args0();
        json_t *ts2 = gsf_call(s, "utc_now_iso", a);
        json_free(a);
        if (gsf_is_raise(ts2)) { json_free(pp); json_free(cur); json_free(payload); free(pn); return ts2; }
        json_set(pp, "updated_at", ts2);
        json_set(pp, "writer_pid", json_copy(json_obj_get(cur, "pid")));
        json_set(pp, "writer_start_time", json_copy(json_obj_get(cur, "start_time")));
        json_set(plats, platform, pp);
    }
    json_t *prev_copy = previous ? json_copy(previous) : json_null();
    json_set(st, "state", json_copy(payload));
    json_t *out = json_array();
    json_append(out, json_string(pn));
    json_append(out, payload);
    json_append(out, prev_copy);
    json_free(cur);
    free(pn);
    free(spn);
    return out;
}

/* ── publish_runtime_status ── */
/* PoP: gsf_flow_publish_runtime_status @ gateway/status.py:publish_runtime_status */
json_t *gsf_flow_publish_runtime_status(json_t *st, const json_t *fields, const gsf_seams_t *s) {
    /* _prepare_runtime_status_update(load_existing=False, **fields) */
    json_t *f2 = fields ? json_copy(fields) : json_object();
    json_set(f2, "load_existing", json_bool(false));
    json_t *prep = gsf_flow_prepare_runtime_status(st, f2, s);
    json_free(f2);
    GSF_PROP(prep);
    const json_t *path = json_get(prep, 0);
    const json_t *payload = json_get(prep, 1);
    const json_t *previous = json_get(prep, 2);
    json_t *a = args0();
    json_t *writer = gsf_call(s, "get_runtime_status_writer", a);
    json_free(a);
    if (gsf_is_raise(writer)) { json_free(prep); return writer; }
    json_free(writer); /* writer identity is not inspected; submit is the seam */
    a = args0();
    aset_j(a, "path", path);
    aset_j(a, "payload", payload);
    json_t *gen = gsf_call(s, "writer.submit", a);
    json_free(a);
    if (gsf_is_raise(gen)) { json_free(prep); return gen; }
    /* _emit_runtime_status_transition(previous_payload, payload) — real leaf */
    json_t *em = gsf_emit_runtime_status_transition(previous, payload, s);
    if (gsf_is_raise(em)) { json_free(gen); json_free(prep); return em; }
    json_free(em);
    json_free(prep);
    return gen;
}

/* ── multiplexer_liveness_for_profile ──
 * Fixture domain: profile_name_for_home scripts a string/null; topology is an
 * object {pid, home, ...} or null; home/root scripts are strings or null. */
/* PoP: gsf_flow_multiplexer_liveness @ gateway/status.py:multiplexer_liveness_for_profile */
json_t *gsf_flow_multiplexer_liveness(const char *profile_dir, const gsf_seams_t *s) {
    char *pd = path_norm(profile_dir);
    char *launch_home = NULL;
    json_t *a = args0(); aset_s(a, "profile_home", pd);
    json_t *name_r = gsf_call(s, "profile_name_for_home", a);
    json_free(a);
    if (gsf_is_raise(name_r)) { free(pd); return name_r; }
    if (!jtruth(name_r)) { json_free(name_r); free(pd); return NULL; }
    const char *name = jstr(name_r);
    bool named = !name || strcmp(name, "default") != 0;
    if (named) {
        /* not _same_hermes_home(profile_dir, get_default_hermes_root() / "profiles" / name) */
        a = args0();
        json_t *root = gsf_call(s, "get_default_hermes_root", a);
        json_free(a);
        if (gsf_is_raise(root)) { json_free(name_r); free(pd); return root; }
        if (!jstr(root)) {
            char *m = xsf("unsupported operand type(s) for /: '%s' and 'str'", py_type_name(root));
            json_t *r = raise_typeerror(m);
            free(m); json_free(root); json_free(name_r); free(pd);
            return r;
        }
        if (!name) {
            char *m = xsf("unsupported operand type(s) for /: 'str' and '%s'", py_type_name(name_r));
            json_t *r = raise_typeerror(m);
            free(m); json_free(root); json_free(name_r); free(pd);
            return r;
        }
        char *j1 = path_join(jstr(root), "profiles");
        char *joined = path_join(j1, name);
        free(j1);
        json_free(root);
        a = args0(); aset_s(a, "left", pd); aset_s(a, "right", joined);
        json_t *same = gsf_call(s, "same_hermes_home", a);
        json_free(a);
        free(joined);
        if (gsf_is_raise(same)) { json_free(name_r); free(pd); return same; }
        bool ok = jtruth(same);
        json_free(same);
        if (!ok) { json_free(name_r); free(pd); return NULL; }
    }
    a = args0();
    json_t *topo = gsf_call(s, "host_gateway_topology", a);
    json_free(a);
    if (gsf_is_raise(topo)) { json_free(name_r); free(pd); return topo; }
    /* launch_home = get_default_hermes_root() — unconditional second call */
    a = args0();
    json_t *root2 = gsf_call(s, "get_default_hermes_root", a);
    json_free(a);
    if (gsf_is_raise(root2)) { json_free(topo); json_free(name_r); free(pd); return root2; }
    if (jstr(root2)) launch_home = strdup(jstr(root2));
    else if (root2 && root2->type != JSON_NULL) launch_home = py_str(root2);
    json_free(root2);
    json_t *pid = NULL;
    bool served_branch = false;
    if (topo) { /* topology is not None */
        if (!is_obj(topo)) {
            json_t *r = raise_attr(topo, "serves");
            json_free(topo); json_free(name_r); free(pd); free(launch_home);
            return r;
        }
        a = args0(); aset_s(a, "name", name);
        json_t *serves = gsf_call(s, "topology.serves", a);
        json_free(a);
        if (gsf_is_raise(serves)) { json_free(topo); json_free(name_r); free(pd); free(launch_home); return serves; }
        bool sv = jtruth(serves);
        json_free(serves);
        if (sv) {
            served_branch = true;
            const json_t *tp = json_obj_get(topo, "pid");
            pid = tp ? json_copy(tp) : NULL;
            const json_t *th = json_obj_get(topo, "home");
            if (jtruth(th)) { /* launch_home = topology.home or launch_home */
                free(launch_home);
                launch_home = jstr(th) ? strdup(jstr(th)) : py_str(th);
            }
        }
    }
    if (!served_branch) {
        if (named) {
            a = args0(); aset_s(a, "name", name);
            json_t *np = gsf_call(s, "named_profile_served_by_running_multiplexer", a);
            json_free(a);
            if (gsf_is_raise(np)) { json_free(topo); json_free(name_r); free(pd); free(launch_home); return np; }
            bool ok = jtruth(np);
            json_free(np);
            if (ok) {
                a = args0();
                pid = gsf_call(s, "live_default_gateway_pid", a);
                json_free(a);
                if (gsf_is_raise(pid)) { json_free(topo); json_free(name_r); free(pd); free(launch_home); return pid; }
            } else {
                json_free(topo); json_free(name_r); free(pd); free(launch_home);
                return NULL;
            }
        } else {
            json_free(topo); json_free(name_r); free(pd); free(launch_home);
            return NULL;
        }
    }
    json_free(topo);
    if (!jpresent(pid)) {
        json_free(pid); json_free(name_r); free(pd); free(launch_home);
        return NULL;
    }
    if (!launch_home) {
        json_t *r = raise_typeerror("unsupported operand type(s) for /: 'NoneType' and 'str'");
        json_free(pid); json_free(name_r); free(pd);
        return r;
    }
    char *sp = path_join(launch_home, "gateway_state.json");
    a = args0(); aset_s(a, "path", sp);
    free(sp);
    json_t *rt = gsf_call(s, "read_runtime_status", a);
    json_free(a);
    if (gsf_is_raise(rt)) { json_free(pid); json_free(name_r); free(pd); free(launch_home); return rt; }
    json_t *out = json_array();
    json_append(out, pid);
    if (jtruth(rt)) {
        json_append(out, rt);
    } else {
        json_free(rt);
        json_append(out, json_object());
    }
    json_free(name_r); free(pd); free(launch_home);
    return out;
}
