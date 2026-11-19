/* doctor_flow.c — FLOW-core port of the UNCLAIMED doctor-diagnostic surface.
 *
 * Defining modules (PoP credits the DEFINING module, not the re-exporting
 * hermes_cli/doctor.py facade):
 *   hermes_cli/doctor.py        run_doctor, _ack_advisory, _print_summary,
 *                               _login_row, _check_auth_providers,
 *                               _check_api_connectivity
 *   hermes_cli/doctor_report.py _mark, check_bool, Finding.merge, ensure_dir,
 *                               warn_on_error
 *   hermes_cli/doctor_live.py   maybe_run_live_checks, _keyed_probe, _probe_audio
 *   hermes_cli/doctor_state.py  _bits, host_gateway_note, _session_count
 *   hermes_cli/doctor_config.py _endpoint_url, _check_retired_session_reset,
 *                               _check_xai_retirement
 *
 * Universal seam: every collaborator / I/O call goes through
 * doc_seam(io, name, args); the dispatcher appends `name` to io->trace,
 * returns the scripted result, applies "state" effects, and models a raised
 * exception with {"__raise__": {type,message,code?,...}}. An "import_raise"
 * script entry models a failed in-function import: the dispatcher returns
 * {"__raise__": spec, "__untrace__": true} and doc_seam retracts the trace
 * token (the live import statement records nothing before it raises).
 *
 * AUTO cosmetic seams (no script, never fail closed): "print" (captures
 * args.text into io->cap_stdout / cap_stderr per args.file), "color"
 * (identity / NO_COLOR projection, returns args.text), and the report
 * primitives "check_ok" / "check_warn" / "check_fail" / "check_info" /
 * "_section" (capture a {kind,text,detail} row into io->cap_rows). These are
 * CLAIMED siblings (ported elsewhere); seaming them keeps their PoP credit.
 * Every other seam is SCRIPTED and FAIL-CLOSED: a missing script raises
 * RuntimeError("unscripted seam: <name>").
 *
 * io->state is a json OBJECT mirroring caller-held state:
 *   finding   the caller-held Finding {issues,manual_issues,fixed} a check
 *             mutates (run_doctor creates it fresh; merge/ensure_dir/the
 *             check bodies extend it).
 *
 * Representation contract (both sides): paths are posix strings; an uncaught
 * exception rides as {"__raise__": {type,message,code?,...}}; SystemExit
 * carries code; the BaseException trio (SystemExit/KeyboardInterrupt/
 * GeneratorExit) PASSES THROUGH every except-Exception site. Integral JSON
 * numbers read as int (libjson has no int/float identity). Fixture input
 * domain is ASCII; the non-ASCII glyphs in message CONSTANTS (check/cross/
 * warn/arrow/diamond/box-drawing/emoji) are byte-pinned UTF-8 literals.
 * str.format sites use a single {e} field (the exception message).
 *
 * Ownership: seam results are caller-owned (json_free). io->trace / state /
 * cap_* are harness-owned. doc_seam() frees its args. Every public doc_*
 * function returns a NEW reference (or a raise sentinel).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include "hermes_json.h"

/* ── message-constant glyphs (byte-pinned UTF-8) ───────────────────────── */
#define DOC_CROSS  "\xe2\x9c\x97"   /* ✗ */
#define DOC_CHECK  "\xe2\x9c\x93"   /* ✓ */
#define DOC_WARN   "\xe2\x9a\xa0"   /* ⚠ */
#define DOC_ARROW  "\xe2\x86\x92"   /* → */
#define DOC_DIAMOND "\xe2\x97\x86"  /* ◆ */
#define DOC_EMDASH "\xe2\x80\x94"   /* — */
#define DOC_ELLIPSIS "\xe2\x80\xa6" /* … */
#define DOC_STETH "\xf0\x9f\xa9\xba" /* 🩺 */
#define DOC_PARTY "\xf0\x9f\x8e\x89" /* 🎉 */
#define DOC_BOX_TL "\xe2\x94\x8c"   /* ┌ */
#define DOC_BOX_TR "\xe2\x94\x90"   /* ┐ */
#define DOC_BOX_BL "\xe2\x94\x94"   /* └ */
#define DOC_BOX_BR "\xe2\x94\x98"   /* ┘ */
#define DOC_BOX_H  "\xe2\x94\x80"   /* ─ */
#define DOC_BOX_V  "\xe2\x94\x82"   /* │ */

/* Colors (hermes_cli/colors.py) — ANSI codes passed to the color seam. */
#define DOC_RESET  "\x1b[0m"
#define DOC_BOLD   "\x1b[1m"
#define DOC_DIM    "\x1b[2m"
#define DOC_RED    "\x1b[31m"
#define DOC_GREEN  "\x1b[32m"
#define DOC_YELLOW "\x1b[33m"
#define DOC_CYAN   "\x1b[36m"

/* ── seam dispatcher + trace + state ───────────────────────────────────── */
typedef struct doc_flow_io_t {
    json_t *(*call)(struct doc_flow_io_t *io, const char *name, json_t *args);
    json_t *trace;          /* ARRAY of string tokens (harness-owned) */
    json_t *state;          /* OBJECT (harness-owned) */
    json_t *cap_stdout;     /* ARRAY of captured stdout print text */
    json_t *cap_stderr;     /* ARRAY of captured stderr print text */
    json_t *cap_rows;       /* ARRAY of captured report-primitive rows */
    void   *ctx;
} doc_flow_io_t;

static void doc_trace(doc_flow_io_t *io, const char *token) {
    if (!io || !io->trace || io->trace->type != JSON_ARRAY) return;
    json_append(io->trace, json_string(token));
}

static json_t *doc_call(doc_flow_io_t *io, const char *name, json_t *args) {
    if (io && io->call) return io->call(io, name, args);
    return json_null();
}

static bool doc_raised(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *doc_raise_spec(const json_t *r) {
    if (!doc_raised(r)) return NULL;
    return json_obj_get(r, "__raise__");
}

static json_t *doc_make_raise(const char *type, const char *message) {
    json_t *spec = json_object();
    json_set(spec, "type", json_string(type));
    json_set(spec, "message", json_string(message ? message : ""));
    json_t *r = json_object();
    json_set(r, "__raise__", spec);
    return r;
}

static json_t *doc_make_raise_code(const char *type, double code) {
    json_t *spec = json_object();
    json_set(spec, "type", json_string(type));
    char buf[32];
    snprintf(buf, sizeof buf, "%.0f", code);
    json_set(spec, "message", json_string(buf));
    json_set(spec, "code", json_number(code));
    json_t *r = json_object();
    json_set(r, "__raise__", spec);
    return r;
}

/* The BaseException trio that passes through every except-Exception site. */
static bool doc_is_base_exc(const char *type) {
    if (!type) return false;
    return strcmp(type, "SystemExit") == 0
        || strcmp(type, "KeyboardInterrupt") == 0
        || strcmp(type, "GeneratorExit") == 0;
}

static bool doc_spec_is_base(const json_t *r) {
    json_t *spec = doc_raise_spec(r);
    if (!spec) return false;
    return doc_is_base_exc(json_get_str(spec, "type", ""));
}

/* seam: trace the attempt, dispatch, free args. An "__untrace__" result
 * (failed in-function import) retracts the trace token but KEEPS the flag. */
static json_t *doc_seam(doc_flow_io_t *io, const char *name, json_t *args) {
    doc_trace(io, name);
    json_t *r = doc_call(io, name, args);
    if (args) json_free(args);
    if (!r) return json_null();
    if (r->type == JSON_OBJECT && json_obj_get(r, "__untrace__")) {
        if (io && io->trace && io->trace->type == JSON_ARRAY && io->trace->c.count > 0) {
            size_t last = io->trace->c.count - 1;
            json_free(io->trace->c.items[last]);
            io->trace->c.count = last;
        }
        json_t *spec = json_obj_get(r, "__raise__");
        json_t *out = json_object();
        json_set(out, "__raise__", spec ? json_copy(spec) : json_null());
        json_set(out, "__untrace__", json_bool(true));
        json_free(r);
        return out;
    }
    return r;
}

/* ── Python-faithful helpers ───────────────────────────────────────────── */
static bool doc_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}

static bool doc_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool doc_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool doc_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool doc_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }

static const char *doc_sval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

/* Python type(x).__name__ for a JSON value (integral numbers read as int). */
static const char *doc_py_type_name(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
        case JSON_NULL: return "NoneType";
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e15) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}

static char *doc_join(const char *a, const char *b) {
    size_t na = strlen(a ? a : ""), nb = strlen(b ? b : "");
    char *out = malloc(na + nb + 1);
    if (!out) return NULL;
    if (na) memcpy(out, a, na);
    if (nb) memcpy(out + na, b, nb);
    out[na + nb] = '\0';
    return out;
}

/* malloc'd printf-style concat (caller frees). */
static char *doc_fmt(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return NULL; }
    char *out = malloc((size_t)n + 1);
    if (out) vsnprintf(out, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return out;
}

static bool doc_startswith(const char *s, const char *pre) {
    if (!s || !pre) return false;
    return strncmp(s, pre, strlen(pre)) == 0;
}

static bool doc_endswith(const char *s, const char *suf) {
    if (!s || !suf) return false;
    size_t ns = strlen(s), nf = strlen(suf);
    return ns >= nf && strcmp(s + ns - nf, suf) == 0;
}

/* ASCII lower (fixture domain is ASCII). malloc'd. */
static char *doc_lower(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)p[i]);
    out[n] = '\0';
    return out;
}

/* str.strip() — ASCII whitespace set (fixture domain ASCII). malloc'd. */
static char *doc_strip(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    size_t a = 0, b = n;
    while (a < b && (p[a]==' '||p[a]=='\t'||p[a]=='\n'||p[a]=='\r'||p[a]=='\v'||p[a]=='\f')) a++;
    while (b > a && (p[b-1]==' '||p[b-1]=='\t'||p[b-1]=='\n'||p[b-1]=='\r'||p[b-1]=='\v'||p[b-1]=='\f')) b--;
    char *out = malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, p + a, b - a);
    out[b - a] = '\0';
    return out;
}

/* str.rstrip(chars) for a single char set. malloc'd. */
static char *doc_rstrip(const char *s, const char *chars) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    while (n > 0 && strchr(chars, p[n-1])) n--;
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, p, n);
    out[n] = '\0';
    return out;
}

/* str.format(e=msg) — replace every literal "{e}" with msg (fixture domain
 * uses only the single {e} field). malloc'd. */
static char *doc_format_e(const char *text, const char *msg) {
    const char *t = text ? text : "";
    const char *m = msg ? msg : "";
    size_t mt = strlen(m);
    size_t cap = strlen(t) + 1;
    /* count occurrences to size the buffer */
    size_t occ = 0;
    for (const char *p = t; (p = strstr(p, "{e}")) != NULL; p += 3) occ++;
    cap += occ * (mt > 3 ? mt - 3 : 0);
    char *out = malloc(cap + 1);
    if (!out) return NULL;
    size_t o = 0;
    const char *p = t;
    while (*p) {
        if (p[0]=='{' && p[1]=='e' && p[2]=='}') {
            memcpy(out + o, m, mt); o += mt; p += 3;
        } else {
            out[o++] = *p++;
        }
    }
    out[o] = '\0';
    return out;
}

/* ── Finding state helpers ─────────────────────────────────────────────── */
static json_t *doc_finding_new(void) {
    json_t *f = json_object();
    json_set(f, "issues", json_array());
    json_set(f, "manual_issues", json_array());
    json_set(f, "fixed", json_number(0));
    return f;
}

static json_t *doc_finding_get(doc_flow_io_t *io) {
    json_t *f = io && io->state ? json_obj_get(io->state, "finding") : NULL;
    if (!f || f->type != JSON_OBJECT) {
        f = doc_finding_new();
        if (io && io->state) json_set(io->state, "finding", f);
    }
    return f;
}

static void doc_arr_extend(json_t *dst, const json_t *src) {
    if (!dst || dst->type != JSON_ARRAY || !src || src->type != JSON_ARRAY) return;
    for (size_t i = 0; i < src->c.count; i++)
        json_append(dst, json_copy(src->c.items[i]));
}

/* Finding.merge: self.issues.extend(other.issues); manual likewise; fixed += . */
static void doc_finding_merge_into(json_t *self, const json_t *other) {
    if (!self || !other || other->type != JSON_OBJECT) return;
    doc_arr_extend(json_obj_get(self, "issues"), json_obj_get(other, "issues"));
    doc_arr_extend(json_obj_get(self, "manual_issues"), json_obj_get(other, "manual_issues"));
    json_t *fx = json_obj_get(self, "fixed");
    double cur = (fx && fx->type == JSON_NUMBER) ? fx->num_val : 0.0;
    json_set(self, "fixed", json_number(cur + json_get_num(other, "fixed", 0)));
}

/* ── report-primitive seam call (AUTO row capture) ─────────────────────── */
static void doc_report(doc_flow_io_t *io, const char *kind, const char *text, const char *detail) {
    json_t *a = json_object();
    json_set(a, "text", json_string(text ? text : ""));
    json_set(a, "detail", json_string(detail ? detail : ""));
    json_t *r = doc_seam(io, kind, a);
    if (r) json_free(r);
}

static void doc_print(doc_flow_io_t *io, const char *text) {
    json_t *a = json_object();
    json_set(a, "text", json_string(text ? text : ""));
    json_t *r = doc_seam(io, "print", a);
    if (r) json_free(r);
}

static void doc_print_empty(doc_flow_io_t *io) { doc_print(io, ""); }

/* color seam (identity). Returns a malloc'd copy of args.text. */
static char *doc_color(doc_flow_io_t *io, const char *text, const char *code) {
    json_t *a = json_object();
    json_set(a, "text", json_string(text ? text : ""));
    json_set(a, "code", json_string(code ? code : ""));
    json_t *r = doc_seam(io, "color", a);
    char *out = strdup(doc_sval(r));
    if (r) json_free(r);
    return out;
}

/* ════════════════════════════════════════════════════════════════════════
 * hermes_cli/doctor_report.py
 * ════════════════════════════════════════════════════════════════════════ */

/* _mark(glyph, col) -> a closure marker. The closure body is doc_mark_call. */
/* PoP: doc_mark @ hermes_cli/doctor_report.py:_mark */
json_t *doc_mark(doc_flow_io_t *io, const char *glyph, const char *col) {
    (void)io;
    json_t *c = json_object();
    json_set(c, "__closure__", json_string("mark"));
    json_set(c, "glyph", json_string(glyph ? glyph : ""));
    json_set(c, "col", json_string(col ? col : ""));
    return c;
}

/* The _mark lambda body: print(f"  {color(glyph,col)} {text}" +
 * (f" {color(detail,DIM)}" if detail else "")). Returns None. */
static json_t *doc_mark_call(doc_flow_io_t *io, const json_t *closure,
                             const char *text, const char *detail) {
    const char *glyph = json_get_str(closure, "glyph", "");
    const char *col = json_get_str(closure, "col", "");
    char *g = doc_color(io, glyph, col);
    char *head = doc_fmt("  %s %s", g, text ? text : "");
    free(g);
    char *line = NULL;
    if (detail && detail[0]) {
        char *d = doc_color(io, detail, DOC_DIM);
        line = doc_fmt("%s %s", head, d);
        free(d);
    } else {
        line = strdup(head);
    }
    free(head);
    doc_print(io, line);
    free(line);
    return json_null();
}

/* PoP: doc_check_bool @ hermes_cli/doctor_report.py:check_bool */
json_t *doc_check_bool(doc_flow_io_t *io, const json_t *cond, const json_t *ok,
                       const json_t *bad, bool fail) {
    bool c = doc_truthy(cond);
    const json_t *args = c ? ok : bad;
    const char *text = "";
    const char *detail = "";
    if (doc_is_str(args)) {
        text = doc_sval(args);
    } else if (doc_is_arr(args)) {
        text = doc_sval(json_get(args, 0));
        detail = doc_sval(json_get(args, 1));
    }
    const char *kind = c ? "check_ok" : (fail ? "check_fail" : "check_warn");
    doc_report(io, kind, text, detail);
    return json_bool(c);
}

/* PoP: doc_finding_merge @ hermes_cli/doctor_report.py:merge */
json_t *doc_finding_merge(doc_flow_io_t *io, const json_t *other) {
    json_t *self = doc_finding_get(io);
    doc_finding_merge_into(self, other);
    return json_null();
}

/* PoP: doc_ensure_dir @ hermes_cli/doctor_report.py:ensure_dir */
json_t *doc_ensure_dir(doc_flow_io_t *io, bool should_fix, const char *path,
                       const char *exists_msg, const char *created_msg,
                       const char *missing_msg) {
    json_t *f = doc_finding_get(io);
    json_t *a = json_object();
    json_set(a, "path", json_string(path ? path : ""));
    json_t *ex = doc_seam(io, "path_exists", a);
    if (doc_raised(ex)) return ex;
    bool exists = doc_truthy(ex);
    json_free(ex);
    if (exists) {
        doc_report(io, "check_ok", exists_msg, "");
    } else if (should_fix) {
        json_t *ma = json_object();
        json_set(ma, "path", json_string(path ? path : ""));
        json_set(ma, "parents", json_bool(true));
        json_set(ma, "exist_ok", json_bool(true));
        json_t *mr = doc_seam(io, "path_mkdir", ma);
        if (doc_raised(mr)) return mr;
        json_free(mr);
        doc_report(io, "check_ok", created_msg, "");
        json_t *fx = json_obj_get(f, "fixed");
        double cur = (fx && fx->type == JSON_NUMBER) ? fx->num_val : 0.0;
        json_set(f, "fixed", json_number(cur + 1));
    } else {
        doc_report(io, "check_warn", missing_msg, "(will be created on first use)");
    }
    return json_null();
}

/* PoP: doc_warn_on_error @ hermes_cli/doctor_report.py:warn_on_error */
json_t *doc_warn_on_error(doc_flow_io_t *io, const char *text, const char *detail,
                          const char *report_kind, const char *body_seam) {
    json_t *r = doc_seam(io, body_seam, json_object());
    if (doc_raised(r)) {
        if (doc_spec_is_base(r)) return r;   /* BaseException propagates */
        json_t *spec = doc_raise_spec(r);
        const char *msg = json_get_str(spec, "message", "");
        if (text && text[0]) {
            char *ft = doc_format_e(text, msg);
            char *fd = doc_format_e(detail ? detail : "({e})", msg);
            doc_report(io, report_kind ? report_kind : "check_warn", ft, fd);
            free(ft); free(fd);
        }
        json_free(r);
        return json_null();
    }
    json_free(r);
    return json_null();
}/* ── extra string helpers (doctor.py message builders) ─────────────────── */

/* Python repr() of a str: pick the delimiter to minimise escapes. malloc'd. */
static char *doc_repr_str(const char *s) {
    const char *p = s ? s : "";
    bool has_sq = strchr(p, '\'') != NULL;
    bool has_dq = strchr(p, '"') != NULL;
    char delim = (has_sq && !has_dq) ? '"' : '\'';
    size_t n = strlen(p);
    char *out = malloc(n * 4 + 3);
    if (!out) return NULL;
    size_t o = 0;
    out[o++] = delim;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)p[i];
        if (c == '\\') { out[o++]='\\'; out[o++]='\\'; }
        else if (c == (unsigned char)delim) { out[o++]='\\'; out[o++]=(char)c; }
        else if (c == '\n') { out[o++]='\\'; out[o++]='n'; }
        else if (c == '\r') { out[o++]='\\'; out[o++]='r'; }
        else if (c == '\t') { out[o++]='\\'; out[o++]='t'; }
        else if (c < 0x20) { o += (size_t)sprintf(out+o, "\\x%02x", c); }
        else out[o++] = (char)c;
    }
    out[o++] = delim;
    out[o] = '\0';
    return out;
}

/* sorted unique join of a JSON string array (set semantics). malloc'd. */
static char *doc_sorted_unique_join(const json_t *arr, const char *sep) {
    size_t n = doc_is_arr(arr) ? arr->c.count : 0;
    const char **v = malloc((n + 1) * sizeof(char *));
    size_t m = 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *e = json_get(arr, i);
        if (!doc_is_str(e)) continue;
        const char *s = doc_sval(e);
        bool dup = false;
        for (size_t j = 0; j < m; j++) if (strcmp(v[j], s)==0) { dup=true; break; }
        if (!dup) v[m++] = s;
    }
    for (size_t i = 1; i < m; i++) {
        const char *key = v[i]; size_t j = i;
        while (j > 0 && strcmp(v[j-1], key) > 0) { v[j]=v[j-1]; j--; }
        v[j]=key;
    }
    size_t seplen = strlen(sep);
    size_t cap = 1;
    for (size_t i = 0; i < m; i++) cap += strlen(v[i]) + seplen;
    char *out = malloc(cap + 1);
    if (!out) { free(v); return NULL; }
    size_t o = 0;
    for (size_t i = 0; i < m; i++) {
        if (i) { memcpy(out+o, sep, seplen); o += seplen; }
        size_t l = strlen(v[i]); memcpy(out+o, v[i], l); o += l;
    }
    out[o] = '\0';
    free(v);
    return out;
}

static bool doc_str_in_arr(const json_t *arr, const char *s) {
    size_t n = doc_is_arr(arr) ? arr->c.count : 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *e = json_get(arr, i);
        if (doc_is_str(e) && strcmp(doc_sval(e), s)==0) return true;
    }
    return false;
}

/* "".join(f"  {i}. {issue}\n" for i,issue in enumerate(remaining,1)). malloc'd. */
static char *doc_numbered(const json_t *remaining) {
    size_t n = doc_is_arr(remaining) ? remaining->c.count : 0;
    size_t cap = 1;
    for (size_t i = 0; i < n; i++) cap += strlen(doc_sval(json_get(remaining, i))) + 32;
    char *out = malloc(cap + 1);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        o += (size_t)sprintf(out+o, "  %zu. %s\n", i+1, doc_sval(json_get(remaining, i)));
    }
    out[o] = '\0';
    return out;
}

/* concat two JSON arrays into a NEW array (list + list). */
static json_t *doc_arr_concat(const json_t *a, const json_t *b) {
    json_t *out = json_array();
    if (doc_is_arr(a)) for (size_t i=0;i<a->c.count;i++) json_append(out, json_copy(a->c.items[i]));
    if (doc_is_arr(b)) for (size_t i=0;i<b->c.count;i++) json_append(out, json_copy(b->c.items[i]));
    return out;
}

/* repeat a UTF-8 literal n times. malloc'd. */
static char *doc_repeat(const char *unit, size_t n) {
    size_t u = strlen(unit);
    char *out = malloc(u*n + 1);
    if (!out) return NULL;
    for (size_t i=0;i<n;i++) memcpy(out + i*u, unit, u);
    out[u*n] = '\0';
    return out;
}

/* color + print helper: print(color(text, code)). */
static void doc_color_print(doc_flow_io_t *io, const char *text, const char *code) {
    char *c = doc_color(io, text, code);
    doc_print(io, c);
    free(c);
}

/* ════════════════════════════════════════════════════════════════════════
 * hermes_cli/doctor.py
 * ════════════════════════════════════════════════════════════════════════ */

/* PoP: doc_login_row @ hermes_cli/doctor.py:_login_row */
json_t *doc_login_row(doc_flow_io_t *io, const char *label, const json_t *status,
                      const char *ok_detail, bool show_error) {
    const json_t *logged = json_obj_get(status, "logged_in");
    json_t *ok = json_array();
    json_append(ok, json_string(label));
    json_append(ok, json_string(ok_detail ? ok_detail : "(logged in)"));
    json_t *bad = json_array();
    json_append(bad, json_string(label));
    json_append(bad, json_string("(not logged in)"));
    json_t *lb = doc_check_bool(io, logged, ok, bad, false);
    json_free(ok); json_free(bad);
    bool logged_in = lb->type == JSON_BOOL && lb->bool_val;
    if (!logged_in && show_error) {
        const json_t *err = json_obj_get(status, "error");
        if (doc_truthy(err)) doc_report(io, "check_info", doc_sval(err), "");
    }
    return lb;
}

/* PoP: doc_print_summary @ hermes_cli/doctor.py:_print_summary */
json_t *doc_print_summary(doc_flow_io_t *io, bool should_fix) {
    json_t *total = doc_finding_get(io);
    doc_print_empty(io);
    json_t *remaining = doc_arr_concat(json_obj_get(total, "issues"),
                                       json_obj_get(total, "manual_issues"));
    size_t nrem = remaining->c.count;
    char *numbered = doc_numbered(remaining);
    double fixed = json_get_num(total, "fixed", 0);
    char *bar = doc_repeat(DOC_BOX_H, 60);
    if (should_fix && fixed > 0) {
        doc_color_print(io, bar, DOC_GREEN);
        char *m1 = doc_fmt("  Fixed %.0f issue(s).", fixed);
        doc_color_print(io, m1, DOC_GREEN);
        free(m1);
        if (nrem > 0) {
            char *m2 = doc_fmt(" %zu issue(s) require manual intervention.", nrem);
            doc_color_print(io, m2, DOC_YELLOW);
            free(m2);
        } else {
            doc_print(io, "");
        }
        doc_print_empty(io);
        if (nrem > 0) doc_print(io, numbered);
    } else if (nrem > 0) {
        doc_color_print(io, bar, DOC_YELLOW);
        char *m1 = doc_fmt("  Found %zu issue(s) to address:", nrem);
        doc_color_print(io, m1, DOC_YELLOW);
        free(m1);
        doc_print_empty(io);
        doc_print(io, numbered);
        json_t *issues = json_obj_get(total, "issues");
        if (!should_fix && doc_is_arr(issues) && issues->c.count > 0) {
            doc_color_print(io, "  Tip: run 'hermes doctor --fix' to auto-fix what's possible.", DOC_DIM);
        }
    } else {
        doc_color_print(io, bar, DOC_GREEN);
        doc_color_print(io, "  All checks passed! " DOC_PARTY, DOC_GREEN);
    }
    doc_print_empty(io);
    free(bar); free(numbered); json_free(remaining);
    return json_null();
}

/* PoP: doc_ack_advisory @ hermes_cli/doctor.py:_ack_advisory */
json_t *doc_ack_advisory(doc_flow_io_t *io, const char *ack_target,
                         const json_t *advisory_ids, const char *dhh) {
    json_t *imp = doc_seam(io, "advisories_import", json_object());
    if (doc_raised(imp)) return imp;
    json_free(imp);
    if (!doc_str_in_arr(advisory_ids, ack_target)) {
        char *repr = doc_repr_str(ack_target);
        char *known = doc_sorted_unique_join(advisory_ids, ", ");
        const char *known_show = (known && known[0]) ? known : "(none)";
        char *msg = doc_fmt("Unknown advisory ID: %s. Known IDs: %s", repr, known_show);
        doc_color_print(io, msg, DOC_RED);
        free(repr); free(known); free(msg);
        return doc_make_raise_code("SystemExit", 2);
    }
    json_t *a = json_object();
    json_set(a, "ack_target", json_string(ack_target));
    json_t *ack = doc_seam(io, "ack_advisory", a);
    if (doc_raised(ack)) return ack;
    bool ok = doc_truthy(ack);
    json_free(ack);
    if (ok) {
        char *msg = doc_fmt("  " DOC_CHECK " Acknowledged advisory %s. It will no longer trigger startup banners.", ack_target);
        doc_color_print(io, msg, DOC_GREEN);
        free(msg);
        return json_null();
    }
    char *msg = doc_fmt("  " DOC_CROSS " Could not save the acknowledgement for %s. Make sure %s/config.yaml is "
                        "writable (`hermes config path` prints the exact file), then re-run "
                        "`hermes doctor --ack %s`.", ack_target, dhh ? dhh : "", ack_target);
    doc_color_print(io, msg, DOC_RED);
    free(msg);
    return doc_make_raise_code("SystemExit", 1);
}/* run_doctor banner lines (byte-pinned from doctor.py:184-186). */
static char *doc_banner(int which) {
    char *h = doc_repeat(DOC_BOX_H, 57);
    char *out = NULL;
    if (which == 0) out = doc_fmt("%s%s%s", DOC_BOX_TL, h, DOC_BOX_TR);
    else if (which == 2) out = doc_fmt("%s%s%s", DOC_BOX_BL, h, DOC_BOX_BR);
    else {
        char *s17 = doc_repeat(" ", 17);
        char *s24 = doc_repeat(" ", 24);
        out = doc_fmt("%s%s%s Hermes Doctor%s%s", DOC_BOX_V, s17, DOC_STETH, s24, DOC_BOX_V);
        free(s17); free(s24);
    }
    free(h);
    return out;
}

/* PoP: doc_check_auth_providers @ hermes_cli/doctor.py:_check_auth_providers */
json_t *doc_check_auth_providers(doc_flow_io_t *io, bool should_fix) {
    (void)should_fix;
    json_t *r = NULL, *exc = NULL;
    /* with warn_on_error("Auth provider status", "(could not check: {e})"): */
    r = doc_seam(io, "auth_import", json_object());
    if (doc_raised(r)) { if (doc_spec_is_base(r)) return r; exc = r; goto b1_except; }
    json_free(r);
    r = doc_seam(io, "get_nous_auth_status_local", json_object());
    if (doc_raised(r)) { if (doc_spec_is_base(r)) return r; exc = r; goto b1_except; }
    { json_t *lr = doc_login_row(io, "Nous Portal auth", r, "(logged in)", false); json_free(lr); json_free(r); }
    r = doc_seam(io, "get_codex_auth_status", json_object());
    if (doc_raised(r)) { if (doc_spec_is_base(r)) return r; exc = r; goto b1_except; }
    {
        json_t *lr = doc_login_row(io, "OpenAI Codex auth", r, "(logged in)", true);
        bool lin = lr->type == JSON_BOOL && lr->bool_val;
        json_free(lr); json_free(r);
        if (!lin) {
            json_t *wa = json_object(); json_set(wa, "name", json_string("codex"));
            json_t *w = doc_seam(io, "_safe_which", wa);
            if (doc_raised(w)) { if (doc_spec_is_base(w)) return w; exc = w; goto b1_except; }
            bool found = doc_truthy(w); json_free(w);
            if (!found)
                doc_report(io, "check_info",
                    "codex CLI not installed (optional " DOC_EMDASH
                    " only required to import tokens from an existing Codex CLI login)", "");
        }
    }
    r = doc_seam(io, "get_minimax_oauth_auth_status", json_object());
    if (doc_raised(r)) { if (doc_spec_is_base(r)) return r; exc = r; goto b1_except; }
    {
        if (!doc_is_obj(r)) {
            char *m = doc_fmt("'%s' object has no attribute 'get'", doc_py_type_name(r));
            json_free(r); exc = doc_make_raise("AttributeError", m); free(m); goto b1_except;
        }
        const char *region = json_get_str(r, "region", "global");
        char *det = doc_fmt("(logged in, region=%s)", region);
        json_t *lr = doc_login_row(io, "MiniMax OAuth", r, det, false);
        free(det); json_free(lr); json_free(r);
    }
    goto b1_done;
b1_except:
    {
        json_t *spec = doc_raise_spec(exc);
        const char *msg = json_get_str(spec, "message", "");
        char *ft = doc_format_e("Auth provider status", msg);
        char *fd = doc_format_e("(could not check: {e})", msg);
        doc_report(io, "check_warn", ft, fd);
        free(ft); free(fd); json_free(exc);
    }
b1_done:
    /* with warn_on_error(""): xAI separately */
    r = doc_seam(io, "xai_import", json_object());
    if (doc_raised(r)) { if (doc_spec_is_base(r)) return r; exc = r; goto b2_except; }
    json_free(r);
    r = doc_seam(io, "get_xai_oauth_auth_status", json_object());
    if (doc_raised(r)) { if (doc_spec_is_base(r)) return r; exc = r; goto b2_except; }
    {
        json_t *status = doc_truthy(r) ? r : json_object();   /* get_xai...() or {} */
        json_t *lr = doc_login_row(io, "xAI OAuth", status, "(logged in)", true);
        json_free(lr);
        if (status != r) json_free(status);
        json_free(r);
    }
    goto b2_done;
b2_except:
    json_free(exc);   /* text "" -> no report */
b2_done:
    return json_null();
}

/* PoP: doc_check_api_connectivity @ hermes_cli/doctor.py:_check_api_connectivity */
json_t *doc_check_api_connectivity(doc_flow_io_t *io, bool should_fix) {
    (void)should_fix;
    json_t *f = doc_finding_get(io);
    json_t *probes = doc_seam(io, "build_probes", json_object());
    if (doc_raised(probes)) return probes;
    size_t np = doc_is_arr(probes) ? probes->c.count : 0;
    char *runmsg = doc_fmt("Running %zu connectivity checks in parallel" DOC_ELLIPSIS, np);
    char *colored = doc_color(io, runmsg, DOC_DIM);
    char *line = doc_fmt("  %s", colored);
    doc_print(io, line);
    free(runmsg); free(colored); free(line);
    json_t *pa = json_object(); json_set(pa, "probes", json_copy(probes));
    json_t *results = doc_seam(io, "run_probes", pa);
    json_free(probes);
    if (doc_raised(results)) return results;
    char *sp = doc_repeat(" ", 70);
    char *clear = doc_fmt("\r%s\r", sp);
    doc_print(io, clear);
    free(sp); free(clear);
    size_t nr = doc_is_arr(results) ? results->c.count : 0;
    for (size_t i = 0; i < nr; i++) {
        json_t *rr = json_get(results, i);
        json_t *lines = json_obj_get(rr, "lines");
        size_t nl = doc_is_arr(lines) ? lines->c.count : 0;
        for (size_t j = 0; j < nl; j++) {
            json_t *ln = json_get(lines, j);
            const char *glyph = doc_sval(json_get(ln, 0));
            const char *label = doc_sval(json_get(ln, 1));
            const char *detail = doc_sval(json_get(ln, 2));
            char *msg = doc_fmt("  %s %s", glyph, label);
            if (detail && detail[0]) { char *m2 = doc_fmt("%s %s", msg, detail); free(msg); msg = m2; }
            doc_print(io, msg); free(msg);
        }
        json_t *issues = json_obj_get(rr, "issues");
        if (doc_truthy(issues)) {
            json_t *fa = json_object();
            json_set(fa, "label", json_string(json_get_str(rr, "label", "")));
            json_t *fb = doc_seam(io, "_has_healthy_oauth_fallback_for_apikey_provider", fa);
            if (doc_raised(fb)) { json_free(results); return fb; }
            bool healthy = doc_truthy(fb); json_free(fb);
            if (!healthy) doc_arr_extend(json_obj_get(f, "issues"), issues);
        }
    }
    json_free(results);
    return json_null();
}

/* PoP: doc_run_doctor @ hermes_cli/doctor.py:run_doctor */
json_t *doc_run_doctor(doc_flow_io_t *io, const json_t *args, const json_t *checks,
                       const json_t *advisory_ids, const char *dhh) {
    bool should_fix = doc_truthy(json_obj_get(args, "fix"));
    json_t *ea = json_object();
    json_set(ea, "key", json_string("HERMES_INTERACTIVE"));
    json_set(ea, "value", json_string("1"));
    json_t *er = doc_seam(io, "os_environ_setdefault", ea);
    if (doc_raised(er)) return er;
    json_free(er);
    const json_t *ack = json_obj_get(args, "ack");
    if (doc_truthy(ack)) return doc_ack_advisory(io, doc_sval(ack), advisory_ids, dhh);
    doc_print_empty(io);
    for (int b = 0; b < 3; b++) { char *bl = doc_banner(b); doc_color_print(io, bl, DOC_CYAN); free(bl); }
    json_t *total = doc_finding_new();
    if (io && io->state) json_set(io->state, "finding", total);
    json_t *ih = doc_seam(io, "installs_held", json_object());
    if (doc_raised(ih)) return ih;
    json_free(ih);
    size_t nc = doc_is_arr(checks) ? checks->c.count : 0;
    for (size_t i = 0; i < nc; i++) {
        json_t *ck = json_get(checks, i);
        const char *title = json_get_str(ck, "title", "");
        const char *name = json_get_str(ck, "name", "");
        if (title && title[0]) doc_report(io, "_section", title, "");
        json_t *ca = json_object(); json_set(ca, "should_fix", json_bool(should_fix));
        json_t *cf = doc_seam(io, name, ca);
        if (doc_raised(cf)) return cf;
        doc_finding_merge_into(total, cf);
        json_free(cf);
    }
    /* with warn_on_error(""): maybe_run_live_checks(args, total.manual_issues) */
    json_t *r = NULL, *exc = NULL;
    r = doc_seam(io, "live_import", json_object());
    if (doc_raised(r)) { if (doc_spec_is_base(r)) return r; exc = r; goto live_except; }
    json_free(r);
    {
        json_t *ma = json_object(); json_set(ma, "args", args ? json_copy(args) : json_null());
        json_t *mi = doc_seam(io, "maybe_run_live_checks", ma);
        if (doc_raised(mi)) { if (doc_spec_is_base(mi)) return mi; exc = mi; goto live_except; }
        json_free(mi);
    }
    goto live_done;
live_except:
    json_free(exc);   /* text "" -> no report */
live_done:
    doc_print_summary(io, should_fix);
    json_t *issues = json_obj_get(total, "issues");
    json_t *manual = json_obj_get(total, "manual_issues");
    bool any = (doc_is_arr(issues) && issues->c.count > 0)
            || (doc_is_arr(manual) && manual->c.count > 0);
    return json_number(any ? 1 : 0);
}/* ── helpers for live/state/config leaves ──────────────────────────────── */
static char *doc_upper(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) out[i] = (char)toupper((unsigned char)p[i]);
    out[n] = '\0';
    return out;
}

/* str(x) projection for the _endpoint_url url operand (str/number/bool/null;
 * containers are outside the fixture domain). malloc'd. */
static char *doc_str_of(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return strdup(doc_sval(v));
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e15)
            snprintf(buf, sizeof buf, "%.0f", v->num_val);
        else
            snprintf(buf, sizeof buf, "%.17g", v->num_val);
        return strdup(buf);
    }
    return strdup("");
}

static json_t *doc_probe_result(const char *name, const char *status, const char *detail) {
    json_t *p = json_object();
    json_set(p, "name", json_string(name ? name : ""));
    json_set(p, "status", json_string(status ? status : ""));
    json_set(p, "detail", json_string(detail ? detail : ""));
    return p;
}

/* ════════════════════════════════════════════════════════════════════════
 * hermes_cli/doctor_live.py
 * ════════════════════════════════════════════════════════════════════════ */

/* PoP: doc_maybe_run_live_checks @ hermes_cli/doctor_live.py:maybe_run_live_checks */
json_t *doc_maybe_run_live_checks(doc_flow_io_t *io, const json_t *args) {
    if (!doc_truthy(json_obj_get(args, "live"))) return json_null();
    json_t *a = json_object();
    json_t *issues = io && io->state ? json_obj_get(io->state, "live_issues") : NULL;
    json_set(a, "issues", issues ? json_copy(issues) : json_array());
    json_t *r = doc_seam(io, "run_live_checks", a);
    if (doc_raised(r)) {
        if (doc_spec_is_base(r)) return r;
        json_t *spec = doc_raise_spec(r);
        char *det = doc_fmt("(%s)", json_get_str(spec, "message", ""));
        doc_report(io, "check_warn", "Live backend probes crashed", det);
        free(det); json_free(r);
        return json_null();
    }
    return r;
}

/* PoP: doc_keyed_probe @ hermes_cli/doctor_live.py:_keyed_probe */
json_t *doc_keyed_probe(doc_flow_io_t *io, const char *name, const char *url,
                        const char *env_var, const char *scheme, double timeout) {
    json_t *ga = json_object();
    json_set(ga, "name", json_string(env_var));
    json_set(ga, "default", json_string(""));
    json_t *g = doc_seam(io, "os_getenv", ga);
    if (doc_raised(g)) return g;
    char *key = doc_strip(doc_sval(g));
    json_free(g);
    if (!key || !key[0]) { free(key); return doc_probe_result(name, "skip", "(not configured)"); }
    char *auth = doc_fmt("%s %s", scheme, key);
    json_t *headers = json_object();
    json_set(headers, "Authorization", json_string(auth));
    free(auth);
    json_t *ha = json_object();
    json_set(ha, "url", json_string(url));
    json_set(ha, "headers", headers);
    json_set(ha, "timeout", json_number(timeout));
    json_t *resp = doc_seam(io, "_http_get", ha);
    free(key);
    if (doc_raised(resp)) return resp;
    json_t *ca = json_object();
    json_set(ca, "name", json_string(name));
    json_set(ca, "resp", resp);
    json_set(ca, "key_hint", json_string(env_var));
    json_t *cr = doc_seam(io, "_classify_http", ca);
    return cr;
}

/* _LOCAL_AUDIO_PROVIDERS / _AUDIO_PROBES (doctor_live.py:26-31). */
static const char *DOC_LOCAL_AUDIO[] = {"", "local", "edge", "neutts", "kittentts", "piper", NULL};
typedef struct { const char *provider; const char *url; const char *env_var; const char *scheme; } doc_audio_probe_t;
static const doc_audio_probe_t DOC_AUDIO_PROBES[] = {
    {"openai", "https://api.openai.com/v1/models", "OPENAI_API_KEY", "Bearer"},
    {"groq", "https://api.groq.com/openai/v1/models", "GROQ_API_KEY", "Bearer"},
    {"elevenlabs", "https://api.elevenlabs.io/v1/voices", "ELEVENLABS_API_KEY", "xi"},
    {NULL, NULL, NULL, NULL},
};

/* PoP: doc_probe_audio @ hermes_cli/doctor_live.py:_probe_audio */
json_t *doc_probe_audio(doc_flow_io_t *io, const char *kind, const json_t *config, double timeout) {
    char *name = doc_upper(kind);
    const json_t *section = json_obj_get(config, kind);
    const json_t *prov_node = doc_is_obj(section) ? json_obj_get(section, "provider") : NULL;
    char *prov_raw = doc_strip(doc_sval(prov_node));
    char *provider = doc_lower(prov_raw);
    free(prov_raw);
    bool local = false;
    for (size_t i = 0; DOC_LOCAL_AUDIO[i]; i++)
        if (strcmp(provider, DOC_LOCAL_AUDIO[i]) == 0) { local = true; break; }
    if (local) {
        const char *show = provider[0] ? provider : "local";
        char *det = doc_fmt("(provider '%s' " DOC_EMDASH " no remote backend to probe)", show);
        json_t *p = doc_probe_result(name, "skip", det);
        free(det); free(name); free(provider);
        return p;
    }
    const doc_audio_probe_t *ap = NULL;
    for (size_t i = 0; DOC_AUDIO_PROBES[i].provider; i++)
        if (strcmp(provider, DOC_AUDIO_PROBES[i].provider) == 0) { ap = &DOC_AUDIO_PROBES[i]; break; }
    if (!ap) {
        char *det = doc_fmt("(provider '%s' " DOC_EMDASH " no live probe implemented)", provider);
        json_t *p = doc_probe_result(name, "skip", det);
        free(det); free(name); free(provider);
        return p;
    }
    json_t *ga = json_object();
    json_set(ga, "name", json_string(ap->env_var));
    json_set(ga, "default", json_string(""));
    json_t *g = doc_seam(io, "os_getenv", ga);
    if (doc_raised(g)) { free(name); free(provider); return g; }
    char *key = doc_strip(doc_sval(g));
    json_free(g);
    if (!key || !key[0]) {
        char *det = doc_fmt("(provider '%s' configured but %s is not set)", provider, ap->env_var);
        json_t *p = doc_probe_result(name, "warn", det);
        free(det); free(key); free(name); free(provider);
        return p;
    }
    json_t *headers = json_object();
    if (strcmp(ap->scheme, "xi") == 0) {
        json_set(headers, "xi-api-key", json_string(key));
    } else {
        char *auth = doc_fmt("Bearer %s", key);
        json_set(headers, "Authorization", json_string(auth));
        free(auth);
    }
    json_t *ha = json_object();
    json_set(ha, "url", json_string(ap->url));
    json_set(ha, "headers", headers);
    json_set(ha, "timeout", json_number(timeout));
    json_t *resp = doc_seam(io, "_http_get", ha);
    free(key);
    if (doc_raised(resp)) { free(name); free(provider); return resp; }
    json_t *ca = json_object();
    json_set(ca, "name", json_string(name));
    json_set(ca, "resp", resp);
    json_set(ca, "key_hint", json_string(ap->env_var));
    json_t *result = doc_seam(io, "_classify_http", ca);
    if (doc_raised(result)) { free(name); free(provider); return result; }
    const char *old_detail = json_get_str(result, "detail", "");
    char *new_detail = doc_fmt("(%s) %s", provider, old_detail);
    json_set(result, "detail", json_string(new_detail));
    free(new_detail); free(name); free(provider);
    return result;
}

/* ════════════════════════════════════════════════════════════════════════
 * hermes_cli/doctor_state.py
 * ════════════════════════════════════════════════════════════════════════ */

/* PoP: doc_bits @ hermes_cli/doctor_state.py:_bits */
json_t *doc_bits(doc_flow_io_t *io, const json_t *pairs) {
    json_t *out = json_array();
    size_t n = doc_is_arr(pairs) ? pairs->c.count : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *pair = json_get(pairs, i);
        json_t *value = json_obj_get(pair, "value");
        if (!value || value->type == JSON_NULL) continue;   /* value is not None */
        const char *fmt = json_get_str(pair, "fmt", "");
        json_t *r = doc_seam(io, fmt, json_object());
        if (doc_raised(r)) { json_free(out); return r; }   /* comprehension atomic */
        json_append(out, r);
    }
    return out;
}

/* PoP: doc_host_gateway_note @ hermes_cli/doctor_state.py:host_gateway_note */
json_t *doc_host_gateway_note(doc_flow_io_t *io) {
    json_t *imp = doc_seam(io, "host_topology_import", json_object());
    if (doc_raised(imp)) {
        if (doc_spec_is_base(imp)) return imp;
        json_free(imp); return json_string("");
    }
    json_free(imp);
    json_t *topo = doc_seam(io, "host_gateway_topology", json_object());
    if (doc_raised(topo)) {
        if (doc_spec_is_base(topo)) return topo;
        json_free(topo); return json_string("");
    }
    if (doc_is_null(topo)) { json_free(topo); return json_string(""); }
    json_free(topo);
    /* topology.describe() is OUTSIDE the try -> a raise propagates */
    json_t *desc = doc_seam(io, "topology_describe", json_object());
    if (doc_raised(desc)) return desc;
    char *note = doc_fmt(" (%s)", doc_sval(desc));
    json_free(desc);
    json_t *out = json_string(note);
    free(note);
    return out;
}

/* PoP: doc_session_count @ hermes_cli/doctor_state.py:_session_count */
json_t *doc_session_count(doc_flow_io_t *io, const char *path) {
    json_t *a = json_object();
    json_set(a, "path", json_string(path ? path : ""));
    json_t *store = doc_seam(io, "open_store_admin", a);
    if (doc_raised(store)) return store;
    json_free(store);
    json_t *cnt = doc_seam(io, "session_row_count", json_object());
    return cnt;
}

/* ════════════════════════════════════════════════════════════════════════
 * hermes_cli/doctor_config.py
 * ════════════════════════════════════════════════════════════════════════ */

/* PoP: doc_endpoint_url @ hermes_cli/doctor_config.py:_endpoint_url */
json_t *doc_endpoint_url(doc_flow_io_t *io, const json_t *entry) {
    (void)io;
    const json_t *url = NULL;
    const json_t *api = json_obj_get(entry, "api");
    const json_t *base = json_obj_get(entry, "base_url");
    const json_t *u = json_obj_get(entry, "url");
    if (doc_truthy(api)) url = api;
    else if (doc_truthy(base)) url = base;
    else if (doc_truthy(u)) url = u;
    char *s = url ? doc_str_of(url) : strdup("");
    char *st = doc_strip(s); free(s);
    char *rs = doc_rstrip(st, "/"); free(st);
    char *lo = doc_lower(rs); free(rs);
    json_t *out = json_string(lo);
    free(lo);
    return out;
}

/* PoP: doc_check_retired_session_reset @ hermes_cli/doctor_config.py:_check_retired_session_reset */
json_t *doc_check_retired_session_reset(doc_flow_io_t *io, bool should_fix) {
    (void)should_fix;
    json_t *f = doc_finding_get(io);
    json_t *imp = doc_seam(io, "config_effective_import", json_object());
    if (doc_raised(imp)) return imp;
    json_free(imp);
    imp = doc_seam(io, "session_reset_import", json_object());
    if (doc_raised(imp)) return imp;
    json_free(imp);
    json_t *cfg = doc_seam(io, "load_user_config_effective", json_object());
    if (doc_raised(cfg)) return cfg;
    json_t *ra = json_object(); json_set(ra, "config", cfg);
    json_t *found = doc_seam(io, "retired_reset_policy", ra);
    json_free(cfg);
    if (doc_raised(found)) return found;
    if (doc_is_null(found)) {
        json_free(found);
        doc_report(io, "check_ok", "No idle/daily session_reset policy configured", "");
        return json_null();
    }
    json_t *en = doc_seam(io, "reset_plugin_enabled", json_object());
    if (doc_raised(en)) { json_free(found); return en; }
    bool enabled = doc_truthy(en); json_free(en);
    if (enabled) {
        const char *k = doc_sval(json_get(found, 0));
        const char *mode = doc_sval(json_get(found, 1));
        char *msg = doc_fmt("%s.mode: %s is applied by the session reset plugin", k, mode);
        doc_report(io, "check_ok", msg, "");
        free(msg); json_free(found);
        return json_null();
    }
    json_t *fa = json_object(); json_set(fa, "args", json_copy(found));
    json_t *notice = doc_seam(io, "format_notice", fa);
    if (doc_raised(notice)) { json_free(found); return notice; }
    doc_report(io, "check_warn", doc_sval(notice), "");
    json_free(notice);
    /* f.manual_issues.append(format_notice(*found)) — a SECOND call */
    json_t *fa2 = json_object(); json_set(fa2, "args", json_copy(found));
    json_t *notice2 = doc_seam(io, "format_notice", fa2);
    if (doc_raised(notice2)) { json_free(found); return notice2; }
    json_t *mi = json_obj_get(f, "manual_issues");
    if (doc_is_arr(mi)) json_append(mi, json_copy(notice2));
    json_free(notice2); json_free(found);
    return json_null();
}

/* PoP: doc_check_xai_retirement @ hermes_cli/doctor_config.py:_check_xai_retirement */
json_t *doc_check_xai_retirement(doc_flow_io_t *io, bool should_fix, const char *migration_url) {
    (void)should_fix;
    json_t *f = doc_finding_get(io);
    json_t *imp = doc_seam(io, "config_import", json_object());
    if (doc_raised(imp)) return imp;
    json_free(imp);
    imp = doc_seam(io, "xai_retirement_import", json_object());
    if (doc_raised(imp)) return imp;
    json_free(imp);
    json_t *cfg = doc_seam(io, "load_config", json_object());
    if (doc_raised(cfg)) return cfg;
    json_t *ra = json_object(); json_set(ra, "config", cfg);
    json_t *refs = doc_seam(io, "find_retired_xai_refs", ra);
    json_free(cfg);
    if (doc_raised(refs)) return refs;
    if (!doc_truthy(refs)) {
        json_free(refs);
        doc_report(io, "check_ok", "No retired xAI models in config", "");
        return json_null();
    }
    size_t n = refs->c.count;
    for (size_t i = 0; i < n; i++) {
        json_t *fa = json_object(); json_set(fa, "ref", json_copy(json_get(refs, i)));
        json_t *issue = doc_seam(io, "format_issue", fa);
        if (doc_raised(issue)) { json_free(refs); return issue; }
        doc_report(io, "check_warn", doc_sval(issue), "");
        json_free(issue);
    }
    char *info = doc_fmt("Migration guide: %s", migration_url ? migration_url : "");
    doc_report(io, "check_info", info, "");
    free(info);
    char *manual = doc_fmt("Update %zu retired xAI model reference(s) in config.yaml " DOC_EMDASH " see %s",
                           n, migration_url ? migration_url : "");
    json_t *mi = json_obj_get(f, "manual_issues");
    if (doc_is_arr(mi)) json_append(mi, json_string(manual));
    free(manual); json_free(refs);
    return json_null();
}