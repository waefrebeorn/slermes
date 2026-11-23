/*
 * session_policy_flow4.c -- gateway/session_policy.py FLOW cores, fourth wave
 * (universal-seam convention, twin of session_policy_flow2 / flow3).
 *
 * Ports 10 UNCLAIMED gateway/session_policy.py functions (CRLF-aware PoP-grep
 * verified across src/ + include/; the 45 flow1+flow2+flow3 claims are SEAMED,
 * never re-claimed). Cluster: launch policy construction, launch overrides,
 * toolset resolution, named-profile cwd, the Desktop-local-turn markers, and
 * the read-only context-var accessors:
 *   build:   build_policy
 *   cwd:     _profile_workspace_cwd
 *   launch:  _apply_launch_overrides
 *   toolsets:_resolve_toolsets, _unnamed_extension_toolsets
 *   desktop: local_desktop_turn, _serving_local_desktop_within
 *   context: profile_read_scope, turn_model, active_policy
 *
 * Every live I/O (foreign-module helpers, claimed siblings, path probes,
 * context-manager enter/exit) enters through sp4_seams_t.call(ctx,name,args);
 * args is OWNED by sp4_call (freed after the borrowed call); results are NEW
 * json_t with NULL == Python None and {"__raise__":K,"msg":M} == a raised
 * exception. FULL BUILTIN EXCEPTION HIERARCHY: sp4_exc_isa walks the complete
 * parent table, so `except Exception` and `except ValueError` sites match
 * exactly the same kinds as live Python (BaseException, SystemExit,
 * KeyboardInterrupt, GeneratorExit, CancelledError and BaseExceptionGroup are
 * the only builtins outside Exception; ExceptionGroup is an Exception).
 * Fail-closed: the harness delivers a RuntimeError raise spec for an
 * unscripted seam.
 *
 * Mutable STATE: _apply_launch_overrides / _resolve_toolsets / build_policy
 * mutate a working copy of the input config (out.state.config). All other
 * funcs echo out.state = null. The three context-manager functions drive a
 * scripted `body` seam and a uniform enter/exit trace; a scripted raise spec
 * on `body` or on any exit seam is propagated / suppressed exactly as the
 * live `with` semantics do.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <ctype.h>
#include "libjson/json.h"

static json_t *sp4_last_enabled = NULL;  /* set by sp4_resolve_toolsets */

typedef struct sp4_seams_t {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} sp4_seams_t;

/* -- universal-seam helpers -- */
static json_t *sp4_call(const sp4_seams_t *s, const char *name, json_t *args) {
    json_t *r = (s && s->call) ? s->call(s->ctx, name, args) : NULL;
    json_free(args);
    return r;
}
static bool sp4_raised(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return false;
    const json_t *k = json_obj_get(r, "__raise__");
    return k && k->type == JSON_STRING;
}
static const char *sp4_kind(const json_t *r) {
    const json_t *k = r ? json_obj_get(r, "__raise__") : NULL;
    return (k && k->type == JSON_STRING && k->str_val) ? k->str_val : "";
}
static json_t *sp4_raise(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static bool sp4_truth(const json_t *v) {
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
static const json_t *sp4_oget(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}
static const char *sp4_sval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool sp4_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool sp4_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool sp4_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool sp4_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool sp4_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool sp4_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
static json_t *sp4_args(void) { return json_object(); }
static void sp4_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void sp4_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void sp4_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static const char *sp4_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            if (isfinite(d) && d == (double)(long long)d) return "int";
            return "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}
static bool sp4_scalar_eq(const json_t *a, const json_t *b) {
    bool an = sp4_is_null(a), bn = sp4_is_null(b);
    if (an || bn) return an && bn;
    bool anum = sp4_is_num(a) || sp4_is_bool(a);
    bool bnum = sp4_is_num(b) || sp4_is_bool(b);
    if (anum && bnum) {
        double av = sp4_is_bool(a) ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double bv = sp4_is_bool(b) ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return av == bv;
    }
    if (sp4_is_str(a) && sp4_is_str(b))
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    return false;
}
static bool sp4_deep_eq(const json_t *a, const json_t *b) {
    if (sp4_is_null(a) || sp4_is_null(b)) return sp4_is_null(a) && sp4_is_null(b);
    if (a->type != b->type) {
        bool anum = sp4_is_num(a) || sp4_is_bool(a);
        bool bnum = sp4_is_num(b) || sp4_is_bool(b);
        return anum && bnum && sp4_scalar_eq(a, b);
    }
    switch (a->type) {
        case JSON_BOOL: case JSON_NUMBER: case JSON_STRING: return sp4_scalar_eq(a, b);
        case JSON_ARRAY: {
            if (json_len(a) != json_len(b)) return false;
            for (size_t i = 0; i < json_len(a); i++)
                if (!sp4_deep_eq(json_get(a, i), json_get(b, i))) return false;
            return true;
        }
        case JSON_OBJECT: {
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                const json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !sp4_deep_eq(a->c.items[i], bv)) return false;
            }
            return true;
        }
        default: return false;
    }
}
static char *sp4_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL: return strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (isfinite(d) && d == (double)(long long)d && d < 9.2e18 && d > -9.2e18)
                snprintf(buf, sizeof(buf), "%lld", (long long)d);
            else snprintf(buf, sizeof(buf), "%.17g", d);
            return strdup(buf);
        }
        default: { char *s = json_serialize(v); return s ? s : strdup(""); }
    }
}
static char *sp4_strip(const char *s) {
    if (!s) s = "";
    while (*s && (*s == ' ' || (*s >= '\t' && *s <= '\r'))) s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == ' ' || (s[n-1] >= '\t' && s[n-1] <= '\r'))) n--;
    char *out = malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}
static char *sp4_lower(const char *s) {
    char *out = strdup(s ? s : "");
    if (out) for (char *p = out; *p; p++) *p = (char)tolower((unsigned char)*p);
    return out;
}

/* -- FULL BUILTIN EXCEPTION HIERARCHY -- */
static bool sp4_streq(const char *a, const char *b) { return a && b && strcmp(a, b) == 0; }
/* The only builtin (and quarry) exception kinds NOT under Exception. */
static bool sp4_base_only(const char *k) {
    return sp4_streq(k, "BaseException") || sp4_streq(k, "SystemExit")
        || sp4_streq(k, "KeyboardInterrupt") || sp4_streq(k, "GeneratorExit")
        || sp4_streq(k, "CancelledError") || sp4_streq(k, "BaseExceptionGroup");
}
/* Direct parent of every builtin exception kind (aliases folded). NULL = root. */
static const char *sp4_exc_parent(const char *k) {
    if (!k) return NULL;
    if (sp4_streq(k, "EnvironmentError") || sp4_streq(k, "IOError")) return "OSError";
    if (sp4_streq(k, "CancelledError")) return "BaseException";
    if (sp4_streq(k, "Exception")) return "BaseException";
    if (sp4_streq(k, "BaseException")) return NULL;
    if (sp4_streq(k, "SystemExit") || sp4_streq(k, "KeyboardInterrupt")
        || sp4_streq(k, "GeneratorExit") || sp4_streq(k, "BaseExceptionGroup")) return "BaseException";
    /* ExceptionGroup inherits Exception AND BaseExceptionGroup. */
    if (sp4_streq(k, "ExceptionGroup")) return "Exception";
    if (sp4_streq(k, "ArithmeticError")) return "Exception";
    if (sp4_streq(k, "FloatingPointError") || sp4_streq(k, "OverflowError")
        || sp4_streq(k, "ZeroDivisionError")) return "ArithmeticError";
    if (sp4_streq(k, "AssertionError") || sp4_streq(k, "AttributeError")
        || sp4_streq(k, "BufferError") || sp4_streq(k, "EOFError")
        || sp4_streq(k, "MemoryError") || sp4_streq(k, "ReferenceError")
        || sp4_streq(k, "StopIteration") || sp4_streq(k, "StopAsyncIteration")
        || sp4_streq(k, "SystemError") || sp4_streq(k, "TypeError")) return "Exception";
    if (sp4_streq(k, "ImportError")) return "Exception";
    if (sp4_streq(k, "ModuleNotFoundError")) return "ImportError";
    if (sp4_streq(k, "LookupError")) return "Exception";
    if (sp4_streq(k, "IndexError") || sp4_streq(k, "KeyError")) return "LookupError";
    if (sp4_streq(k, "NameError")) return "Exception";
    if (sp4_streq(k, "UnboundLocalError")) return "NameError";
    if (sp4_streq(k, "OSError")) return "Exception";
    if (sp4_streq(k, "BlockingIOError") || sp4_streq(k, "ChildProcessError")
        || sp4_streq(k, "FileExistsError") || sp4_streq(k, "FileNotFoundError")
        || sp4_streq(k, "InterruptedError") || sp4_streq(k, "IsADirectoryError")
        || sp4_streq(k, "NotADirectoryError") || sp4_streq(k, "PermissionError")
        || sp4_streq(k, "ProcessLookupError") || sp4_streq(k, "TimeoutError")) return "OSError";
    if (sp4_streq(k, "ConnectionError")) return "OSError";
    if (sp4_streq(k, "BrokenPipeError") || sp4_streq(k, "ConnectionAbortedError")
        || sp4_streq(k, "ConnectionRefusedError") || sp4_streq(k, "ConnectionResetError")) return "ConnectionError";
    if (sp4_streq(k, "RuntimeError")) return "Exception";
    if (sp4_streq(k, "NotImplementedError") || sp4_streq(k, "RecursionError")) return "RuntimeError";
    if (sp4_streq(k, "SyntaxError")) return "Exception";
    if (sp4_streq(k, "IndentationError")) return "SyntaxError";
    if (sp4_streq(k, "TabError")) return "IndentationError";
    if (sp4_streq(k, "ValueError")) return "Exception";
    if (sp4_streq(k, "UnicodeError")) return "ValueError";
    if (sp4_streq(k, "UnicodeDecodeError") || sp4_streq(k, "UnicodeEncodeError")
        || sp4_streq(k, "UnicodeTranslateError")) return "UnicodeError";
    if (sp4_streq(k, "Warning")) return "Exception";
    if (sp4_streq(k, "BytesWarning") || sp4_streq(k, "DeprecationWarning")
        || sp4_streq(k, "EncodingWarning") || sp4_streq(k, "FutureWarning")
        || sp4_streq(k, "ImportWarning") || sp4_streq(k, "PendingDeprecationWarning")
        || sp4_streq(k, "ResourceWarning") || sp4_streq(k, "RuntimeWarning")
        || sp4_streq(k, "SyntaxWarning") || sp4_streq(k, "UserWarning")) return "Warning";
    return NULL; /* unknown / custom kind */
}
/* isinstance(exc, parent): full hierarchy walk; unknown kinds are Exception-family. */
static bool sp4_exc_isa(const char *kind, const char *parent) {
    if (!kind || !parent) return false;
    const char *cur = kind;
    for (int i = 0; i < 32 && cur; i++) {
        if (sp4_streq(cur, parent)) return true;
        if (sp4_streq(cur, "ExceptionGroup") && sp4_streq(parent, "BaseExceptionGroup")) return true;
        const char *p = sp4_exc_parent(cur);
        if (!p && i == 0 && !sp4_base_only(cur) && !sp4_streq(cur, "BaseException")) p = "Exception";
        cur = p;
    }
    return false;
}
/* __raise__ spec caught by an `except <parent>` site? */
static bool sp4_caught(const json_t *r, const char *parent) {
    return sp4_exc_isa(sp4_kind(r), parent);
}


/* -- AuthorityRefusal (real class twin) -- */
static bool sp4_py_ws(unsigned char ch) {
    return ch == ' ' || (ch >= '\t' && ch <= '\r') || (ch >= 0x1c && ch <= 0x1f);
}
static char *sp4_one_line(const char *value, size_t limit) {
    if (!value) return NULL;
    size_t n = strlen(value);
    char *tmp = malloc(n + 1);
    if (!tmp) return NULL;
    size_t k = 0;
    bool pending_space = false, started = false;
    for (size_t i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)value[i];
        if (sp4_py_ws(ch)) { if (started) pending_space = true; continue; }
        if (pending_space) { tmp[k++] = ' '; pending_space = false; }
        started = true;
        tmp[k++] = (char)ch;
    }
    tmp[k] = '\0';
    size_t w = 0;
    for (size_t i = 0; i < k; i++) {
        unsigned char ch = (unsigned char)tmp[i];
        if (ch < 0x20 || (ch >= 0x7f && ch <= 0x9f)) continue;
        tmp[w++] = tmp[i];
    }
    tmp[w] = '\0';
    if (w == 0) { free(tmp); return NULL; }
    if (w > limit) w = limit;
    char *out = malloc(w + 1);
    if (!out) { free(tmp); return NULL; }
    memcpy(out, tmp, w); out[w] = '\0';
    free(tmp);
    return out;
}
static json_t *sp4_ar_raise(const char *reason, const char *field, const char *detail) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string("AuthorityRefusal"));
    json_set(r, "msg", json_string(reason ? reason : ""));
    json_set(r, "reason", json_string(reason ? reason : ""));
    char *f = sp4_one_line(field, 80);
    json_set(r, "field", f ? json_string(f) : json_null());
    free(f);
    char *d = sp4_one_line(detail, 160);
    json_set(r, "detail", d ? json_string(d) : json_null());
    free(d);
    return r;
}
/* _refuse / _policy_refused are claimed siblings; their spec is reconstructed
 * inline (compose, never re-claim). */
static json_t *sp4_refuse(const char *field, const char *detail) {
    return sp4_ar_raise("invalid_params", field, detail);
}
static json_t *sp4_jloads_str(const char *str) {
    char *err = NULL;
    json_t *r = json_parse(str ? str : "", &err);
    free(err);
    if (!r) return sp4_raise("ValueError", "invalid JSON (out of domain)");
    return r;
}

/* -- set helpers (deterministic sorted output) -- */
typedef struct { char **v; size_t n, cap; } sp4_set_t;
static void sp4_set_init(sp4_set_t *s) { s->v = NULL; s->n = s->cap = 0; }
static bool sp4_set_has(const sp4_set_t *s, const char *x) {
    for (size_t i = 0; i < s->n; i++) if (strcmp(s->v[i], x) == 0) return true;
    return false;
}
static void sp4_set_add(sp4_set_t *s, const char *x) {
    if (!x || sp4_set_has(s, x)) return;
    if (s->n == s->cap) { s->cap = s->cap ? s->cap * 2 : 8; s->v = realloc(s->v, s->cap * sizeof(char *)); }
    s->v[s->n++] = strdup(x);
}
static void sp4_set_add_json(sp4_set_t *s, const json_t *v) {
    char *p = sp4_pystr(v);
    sp4_set_add(s, p);
    free(p);
}
static void sp4_set_free(sp4_set_t *s) {
    for (size_t i = 0; i < s->n; i++) free(s->v[i]);
    free(s->v); s->v = NULL; s->n = s->cap = 0;
}
static void sp4_set_sort(sp4_set_t *s) {
    for (size_t i = 1; i < s->n; i++) {
        char *t = s->v[i]; size_t j = i;
        while (j > 0 && strcmp(s->v[j-1], t) > 0) { s->v[j] = s->v[j-1]; j--; }
        s->v[j] = t;
    }
}
static json_t *sp4_set_array(sp4_set_t *s) {
    sp4_set_sort(s);
    json_t *a = json_array();
    for (size_t i = 0; i < s->n; i++) json_append(a, json_string(s->v[i]));
    return a;
}
static void sp4_set_from_arr(sp4_set_t *s, const json_t *arr) {
    if (sp4_is_arr(arr)) for (size_t i = 0; i < json_len(arr); i++) sp4_set_add_json(s, json_get(arr, i));
}
static bool sp4_set_subset(const sp4_set_t *a, const sp4_set_t *b) {
    for (size_t i = 0; i < a->n; i++) if (!sp4_set_has(b, a->v[i])) return false;
    return true;
}
static void sp4_set_union(sp4_set_t *out, const sp4_set_t *a, const sp4_set_t *b) {
    for (size_t i = 0; i < a->n; i++) sp4_set_add(out, a->v[i]);
    for (size_t i = 0; i < b->n; i++) sp4_set_add(out, b->v[i]);
}
static void sp4_set_sub(sp4_set_t *out, const sp4_set_t *a, const sp4_set_t *b) {
    for (size_t i = 0; i < a->n; i++) if (!sp4_set_has(b, a->v[i])) sp4_set_add(out, a->v[i]);
}
static void sp4_set_intersect(sp4_set_t *out, const sp4_set_t *a, const sp4_set_t *b) {
    for (size_t i = 0; i < a->n; i++) if (sp4_set_has(b, a->v[i])) sp4_set_add(out, a->v[i]);
}

/* -- strings / paths -- */
static char *sp4_join_comma(const char **items, size_t n) {
    size_t cap = 64; char *out = malloc(cap); size_t k = 0; out[0] = '\0';
    for (size_t i = 0; i < n; i++) {
        size_t en = strlen(items[i]);
        while (k + en + 3 >= cap) { cap = (k + en + 3) * 2; out = realloc(out, cap); }
        if (i) { out[k++] = ','; out[k++] = ' '; }
        memcpy(out + k, items[i], en); k += en; out[k] = '\0';
    }
    return out;
}
/* POSIX normpath for absolute paths: collapse //, drop '.', resolve '..'. */
static char *sp4_normpath(const char *p) {
    if (!p) p = "";
    bool absolute = p[0] == '/';
    char *work = strdup(p);
    char **parts = malloc((strlen(work) + 2) * sizeof(char *));
    size_t np = 0;
    char *tok = strtok(work, "/");
    while (tok) {
        if (strcmp(tok, ".") == 0) { /* skip */ }
        else if (strcmp(tok, "..") == 0) {
            if (np > 0 && strcmp(parts[np-1], "..") != 0) { free(parts[--np]); }
            else if (!absolute) parts[np++] = strdup("..");
        } else parts[np++] = strdup(tok);
        tok = strtok(NULL, "/");
    }
    size_t cap = 2; for (size_t i = 0; i < np; i++) cap += strlen(parts[i]) + 1;
    char *out = malloc(cap); size_t k = 0;
    if (absolute) out[k++] = '/';
    for (size_t i = 0; i < np; i++) {
        if (k > 0 && out[k-1] != '/') out[k++] = '/';
        size_t l = strlen(parts[i]); memcpy(out + k, parts[i], l); k += l;
        free(parts[i]);
    }
    if (k == 0) { out[k++] = '.'; }
    out[k] = '\0';
    free(parts); free(work);
    return out;
}
static char *sp4_path_join(const char *a, const char *b) {
    if (!a || !*a) return strdup(b ? b : "");
    /* pathlib drops a leading '.' component: Path('.')/'config.yaml' is
     * 'config.yaml', not './config.yaml'. Treat '.', './', './//' as that base. */
    {
        size_t j = strlen(a);
        while (j > 0 && a[j - 1] == '/') j--;
        if (j == 1 && a[0] == '.') return strdup(b ? b : "");
    }
    size_t n = strlen(a);
    char *out = malloc(n + strlen(b ? b : "") + 2);
    strcpy(out, a);
    size_t k = n;
    while (k > 0 && out[k-1] == '/') out[--k] = '\0';
    out[k++] = '/';
    strcpy(out + k, b ? b : "");
    return out;
}
static bool sp4_is_abs(const char *p) { return p && p[0] == '/'; }

/* -- module constants -- */
static const char *sp4_surface(const char *label) {
    if (!label) return NULL;
    if (strcmp(label, "cli") == 0) return "cli";
    if (strcmp(label, "tui") == 0) return "tui";
    if (strcmp(label, "gui") == 0) return "desktop";
    if (strcmp(label, "acp") == 0) return "acp";
    if (strcmp(label, "tool") == 0) return "cli";
    if (strcmp(label, "oneshot") == 0) return "cli";
    return NULL;
}
static const char *SP4_CREATE_FIELDS[] = {
    "request_id", "source", "cwd", "model", "toolsets", "provider", "base_url",
    "reasoning", "max_turns", "ignore_rules", "api_key", "editor", "yolo", "safe_mode",
    "ignore_user_config", "skills", "checkpoints", "accept_hooks", "pass_session_id",
    "follow_profile_config", "service_tier", "cwd_explicit", "run_budget", "verbose",
    "row_cwd", "client_env", "client"};
#define SP4_N_CREATE (sizeof(SP4_CREATE_FIELDS)/sizeof(SP4_CREATE_FIELDS[0]))
static bool sp4_is_create_field(const char *k) {
    for (size_t i = 0; i < SP4_N_CREATE; i++) if (strcmp(SP4_CREATE_FIELDS[i], k) == 0) return true;
    return false;
}
static const char *SP4_POLICY_FIELDS[] = {
    "source", "platform", "cwd", "model", "toolsets", "config_json", "request_json",
    "terminal_json", "credential_ref", "config_secret_ref", "editor_mcp_json",
    "kanban_json", "safe_mode", "ignore_user_config", "skills_prompt", "coding_posture",
    "client_env_json", "client_env_remote", "client_env_rules_json", "edit_approval_policy"};
#define SP4_N_POLICY (sizeof(SP4_POLICY_FIELDS)/sizeof(SP4_POLICY_FIELDS[0]))
static json_t *sp4_field_default(const char *k) {
    if (sp4_streq(k,"safe_mode") || sp4_streq(k,"ignore_user_config") || sp4_streq(k,"coding_posture")
        || sp4_streq(k,"client_env_remote")) return json_bool(false);
    if (sp4_streq(k,"edit_approval_policy")) return json_string("ask");
    return json_null();
}
static json_t *sp4_policy_norm(const json_t *d) {
    json_t *out = json_object();
    for (size_t i = 0; i < SP4_N_POLICY; i++) {
        const char *k = SP4_POLICY_FIELDS[i];
        const json_t *v = sp4_oget(d, k);
        json_set(out, k, v ? json_copy(v) : sp4_field_default(k));
    }
    return out;
}


/* -- exception/attribute helpers used by the flow cores -- */
static json_t *sp4_attr_err(const json_t *v, const char *attr) {
    char m[160];
    snprintf(m, sizeof(m), "'%s' object has no attribute '%s'", sp4_type_name(v), attr);
    return sp4_raise("AttributeError", m);
}
/* uniform `with` __exit__ handling: masks a pending raise, or suppresses it
 * when the CM returns truthy. Returns true when a pending exception remains. */
static bool sp4_cm_exit(json_t **pending, json_t *exit_ret) {
    if (sp4_raised(exit_ret)) {
        if (*pending) json_free(*pending);
        *pending = exit_ret;
        return true;
    }
    bool sup = sp4_truth(exit_ret);
    json_free(exit_ret);
    if (sup && *pending) { json_free(*pending); *pending = NULL; return false; }
    return *pending != NULL;
}
static void sp4_set_remove(sp4_set_t *s, const sp4_set_t *b) {
    size_t w = 0;
    for (size_t i = 0; i < s->n; i++) {
        if (sp4_set_has(b, s->v[i])) free(s->v[i]);
        else s->v[w++] = s->v[i];
    }
    s->n = w;
}

/* -- 1. active_policy -- */
/* PoP: sp4_active_policy @ gateway/session_policy.py:active_policy */
static json_t *sp4_active_policy(const json_t *active) {
    return active ? json_copy(active) : json_null();
}

/* -- 2. turn_model -- */
/* PoP: sp4_turn_model @ gateway/session_policy.py:turn_model */
static json_t *sp4_turn_model(const json_t *policy, const json_t *authority, const sp4_seams_t *s) {
    json_t *a = sp4_args();
    sp4_j(a, "policy", policy);
    sp4_j(a, "authority", authority);
    sp4_b(a, "for_turn", true);
    json_t *live = sp4_call(s, "policy.read_profile", a);
    if (sp4_raised(live)) return live;   /* token set is BEFORE the try */
    json_free(live);
    a = sp4_args();
    sp4_j(a, "policy", policy);
    sp4_j(a, "authority", authority);
    json_t *r = sp4_call(s, "policy.current_model", a);
    return r ? r : json_null();          /* finally reset is not observable */
}

/* -- 3. profile_read_scope (context manager) -- */
/* PoP: sp4_profile_read_scope @ gateway/session_policy.py:profile_read_scope */
static json_t *sp4_profile_read_scope(const json_t *policy, const json_t *authority, const sp4_seams_t *s) {
    json_t *a = sp4_args();
    sp4_j(a, "policy", policy);
    sp4_j(a, "authority", authority);
    sp4_b(a, "for_turn", false);
    json_t *live = sp4_call(s, "policy.read_profile", a);
    if (sp4_raised(live)) return live;   /* __enter__ raised: no finally */
    json_free(live);
    json_t *body = sp4_call(s, "body", sp4_args());
    if (sp4_raised(body)) return body;   /* finally resets token, no visible I/O */
    json_free(body);
    return json_null();
}

/* -- 4. _serving_local_desktop_within (context manager) -- */
/* PoP: sp4_serving_local_desktop_within @ gateway/session_policy.py:_serving_local_desktop_within */
static json_t *sp4_serving_local_desktop_within(const json_t *scope, const sp4_seams_t *s) {
    json_t *a = sp4_args();
    json_t *r = sp4_call(s, "scope.__enter__", a);
    if (sp4_raised(r)) return r;
    json_free(r);
    a = sp4_args(); sp4_b(a, "on", true);
    json_t *e = sp4_call(s, "serving_local_desktop.__enter__", a);
    if (sp4_raised(e)) {
        json_t *pending = e;
        a = sp4_args(); sp4_j(a, "exc", pending);
        sp4_cm_exit(&pending, sp4_call(s, "scope.__exit__", a));
        return pending ? pending : json_null();
    }
    json_free(e);
    json_t *body = sp4_call(s, "body", sp4_args());
    json_t *pending = sp4_raised(body) ? body : NULL;
    if (!pending) json_free(body);
    a = sp4_args(); sp4_j(a, "exc", pending ? pending : json_null());
    sp4_cm_exit(&pending, sp4_call(s, "serving_local_desktop.__exit__", a));
    a = sp4_args(); sp4_j(a, "exc", pending ? pending : json_null());
    sp4_cm_exit(&pending, sp4_call(s, "scope.__exit__", a));
    return pending ? pending : json_null();
}

/* -- 5. local_desktop_turn -- */
/* PoP: sp4_local_desktop_turn @ gateway/session_policy.py:local_desktop_turn */
static json_t *sp4_local_desktop_turn(const json_t *runner, const json_t *source,
                                      const json_t *scope, const sp4_seams_t *s) {
    const json_t *plat = sp4_oget(source, "platform");
    json_t *policy = NULL;
    if (sp4_is_str(plat) && sp4_streq(sp4_sval(plat), "local")) {
        json_t *a = sp4_args(); sp4_j(a, "source", source);
        json_t *ad = sp4_call(s, "runner._adapter_for_source", a);
        if (sp4_raised(ad)) return ad;
        if (sp4_is_obj(ad)) {
            const json_t *policies = sp4_oget(ad, "policies");
            const json_t *cid = sp4_oget(source, "chat_id");
            if (sp4_is_obj(policies) && cid) {
                const char *key = sp4_sval(cid);
                const json_t *p = key ? json_obj_get((json_t *)policies, key) : NULL;
                if (p) policy = json_copy(p);
            }
        }
        json_free(ad);
    }
    const json_t *psrc = sp4_oget(policy, "source");
    if (!sp4_is_obj(policy) || !sp4_is_str(psrc) || !sp4_streq(sp4_sval(psrc), "gui")) {
        json_free(policy);
        return scope ? json_copy(scope) : json_null();
    }
    /* live builds the tuple (policy.config_json, policy.request_json) BEFORE any():
     * a missing attribute raises immediately, in that order. */
    if (!json_has((json_t *)policy, "config_json") || !json_has((json_t *)policy, "request_json")) {
        const char *miss = json_has((json_t *)policy, "config_json") ? "request_json" : "config_json";
        char m[160]; snprintf(m, sizeof(m),
            "'types.SimpleNamespace' object has no attribute '%s'", miss);
        json_t *er = sp4_raise("AttributeError", m); json_free(policy); return er;
    }
    const char *keys[2] = { "config_json", "request_json" };
    for (int i = 0; i < 2; i++) {
        const json_t *raw = sp4_oget(policy, keys[i]);
        if (!sp4_is_str(raw)) continue;
        json_t *doc = sp4_jloads_str(sp4_sval(raw));
        if (sp4_raised(doc)) { json_free(policy); return doc; }
        if (!sp4_is_obj(doc)) { json_t *er = sp4_attr_err(doc, "get"); json_free(doc); json_free(policy); return er; }
        const json_t *m = sp4_oget(doc, "desktop_local");
        bool hit = sp4_is_bool(m) && m->bool_val;
        json_free(doc);
        if (hit) {
            json_free(policy);
            json_t *r = json_object();
            json_set(r, "__cm__", json_string("serving_local_desktop"));
            return r;
        }
    }
    json_free(policy);
    return scope ? json_copy(scope) : json_null();
}

/* -- 6. _profile_workspace_cwd -- */
/* PoP: sp4_profile_workspace_cwd @ gateway/session_policy.py:_profile_workspace_cwd */
static json_t *sp4_profile_workspace_cwd(const char *home, const sp4_seams_t *s) {
    json_t *a = sp4_args(); sp4_s(a, "home", home);
    json_t *name = sp4_call(s, "profile_name_for_home", a);
    if (sp4_raised(name)) return name;
    if (sp4_is_null(name) || (sp4_is_str(name) && sp4_streq(sp4_sval(name), "default"))) {
        json_free(name); return json_null();
    }
    json_free(name);
    char *jp = sp4_path_join(home ? home : "", "config.yaml");
    a = sp4_args(); sp4_s(a, "path", jp);
    json_t *cfg = sp4_call(s, "read_user_config_raw", a);
    free(jp);
    if (sp4_raised(cfg)) return cfg;
    const json_t *terminal = sp4_oget(cfg, "terminal");
    const json_t *cv = sp4_is_obj(terminal) ? sp4_oget(terminal, "cwd") : NULL;
    char *raw = sp4_truth(cv) ? sp4_pystr(cv) : strdup("");
    char *stripped = sp4_strip(raw);
    free(raw);
    json_free(cfg);
    if (sp4_streq(stripped, "") || sp4_streq(stripped, ".") || sp4_streq(stripped, "auto") || sp4_streq(stripped, "cwd")) {
        free(stripped); return json_null();
    }
    char *norm = sp4_normpath(stripped);
    free(stripped);
    a = sp4_args(); sp4_s(a, "path", norm);
    json_t *isdir = sp4_call(s, "os.path.isdir", a);
    json_t *ret = NULL;
    if (sp4_raised(isdir)) ret = isdir;
    else { if (sp4_truth(isdir)) ret = json_string(norm); json_free(isdir); }
    free(norm);
    return ret ? ret : json_null();
}

/* -- 7. _unnamed_extension_toolsets -- */
/* PoP: sp4_unnamed_extension_toolsets @ gateway/session_policy.py:_unnamed_extension_toolsets */
static json_t *sp4_unnamed_extension_toolsets(const json_t *explicit, const json_t *requested,
        const json_t *config, const json_t *builtin, const sp4_seams_t *s) {
    sp4_set_t ex, req, bt;
    sp4_set_init(&ex); sp4_set_init(&req); sp4_set_init(&bt);
    sp4_set_from_arr(&ex, explicit); sp4_set_from_arr(&req, requested); sp4_set_from_arr(&bt, builtin);
    if (sp4_set_has(&ex, "all") || sp4_set_has(&ex, "*")) {
        sp4_set_free(&ex); sp4_set_free(&req); sp4_set_free(&bt);
        return json_array();
    }
    const json_t *configured = sp4_oget(config, "mcp_servers");
    bool has_cfg = sp4_is_obj(configured) && configured->c.count > 0;
    sp4_set_t named; sp4_set_init(&named);
    for (size_t i = 0; i < ex.n; i++) {
        sp4_set_add(&named, ex.v[i]);
        if (strncmp(ex.v[i], "mcp-", 4) == 0 && !(has_cfg && json_has((json_t *)configured, ex.v[i])))
            sp4_set_add(&named, ex.v[i] + 4);
    }
    json_t *a = sp4_args();
    json_t *pk = sp4_call(s, "get_plugin_toolset_keys", a);
    if (sp4_raised(pk)) { sp4_set_free(&ex); sp4_set_free(&req); sp4_set_free(&bt); sp4_set_free(&named); return pk; }
    sp4_set_t plugins; sp4_set_init(&plugins); sp4_set_from_arr(&plugins, pk); json_free(pk);
    sp4_set_t enabled; sp4_set_init(&enabled);
    sp4_set_t inter; sp4_set_init(&inter); sp4_set_intersect(&inter, &plugins, &bt);
    sp4_set_t covered; sp4_set_init(&covered);
    for (size_t i = 0; i < inter.n; i++) {
        a = sp4_args(); sp4_s(a, "name", inter.v[i]); sp4_b(a, "include_registry", true);
        json_t *rt = sp4_call(s, "resolve_toolset", a);
        if (sp4_raised(rt)) {
            sp4_set_free(&ex); sp4_set_free(&req); sp4_set_free(&bt); sp4_set_free(&named);
            sp4_set_free(&plugins); sp4_set_free(&enabled); sp4_set_free(&inter); sp4_set_free(&covered);
            return rt;
        }
        sp4_set_t tools; sp4_set_init(&tools); sp4_set_from_arr(&tools, rt); json_free(rt);
        if (sp4_set_subset(&tools, &req)) sp4_set_add(&covered, inter.v[i]);
        sp4_set_free(&tools);
    }
    a = sp4_args(); sp4_j(a, "config", config);
    json_t *em = sp4_call(s, "enabled_mcp_server_names", a);
    if (sp4_raised(em)) {
        sp4_set_free(&ex); sp4_set_free(&req); sp4_set_free(&bt); sp4_set_free(&named);
        sp4_set_free(&plugins); sp4_set_free(&inter); sp4_set_free(&covered); return em;
    }
    sp4_set_from_arr(&enabled, em); json_free(em);
    sp4_set_t all; sp4_set_init(&all); sp4_set_union(&all, &plugins, &enabled);
    sp4_set_remove(&all, &named);
    sp4_set_remove(&all, &covered);
    json_t *out = sp4_set_array(&all);
    sp4_set_free(&all); sp4_set_free(&covered); sp4_set_free(&inter); sp4_set_free(&enabled);
    sp4_set_free(&plugins); sp4_set_free(&named); sp4_set_free(&ex); sp4_set_free(&req); sp4_set_free(&bt);
    return out;
}


/* -- 8. _resolve_toolsets -- */
static json_t *sp4_repr(const json_t *v) {
    if (sp4_is_str(v)) {
        const char *s = sp4_sval(v);
        size_t n = strlen(s);
        char *out = malloc(n + 3);
        size_t k = 0; out[k++] = '\'';
        for (size_t i = 0; i < n; i++) {
            if (s[i] == '\'' || s[i] == '\\') out[k++] = '\\';
            out[k++] = s[i];
        }
        out[k++] = '\''; out[k] = '\0';
        json_t *r = json_string(out); free(out); return r;
    }
    char *p = sp4_pystr(v);
    json_t *r = json_string(p); free(p); return r;
}
static json_t *sp4_refuse_repr(const char *field, const char *prefix, const json_t *value) {
    char *rp = NULL;
    json_t *rep = sp4_repr(value);
    rp = rep ? strdup(sp4_sval(rep) ? sp4_sval(rep) : "") : strdup("");
    json_free(rep);
    size_t n = strlen(prefix ? prefix : "") + strlen(rp) + 4;
    char *msg = malloc(n);
    snprintf(msg, n, "%s%s", prefix ? prefix : "", rp);
    free(rp);
    json_t *r = sp4_refuse(field, msg);
    free(msg);
    return r;
}
static json_t *sp4_get_obj(json_t *config, const char *key) {
    json_t *v = (json_t *)sp4_oget(config, key);
    if (!sp4_is_obj(v)) {
        v = json_object();
        json_set(config, key, v);
    }
    return v;
}
/* expand a list of toolset names through the resolve_toolset seam */
static json_t *sp4_expand(const json_t *names, bool include_registry, const sp4_seams_t *s) {
    sp4_set_t out; sp4_set_init(&out);
    if (sp4_is_arr(names)) {
        for (size_t i = 0; i < json_len(names); i++) {
            json_t *a = sp4_args();
            sp4_j(a, "name", json_get(names, i));
            sp4_b(a, "include_registry", include_registry);
            json_t *r = sp4_call(s, "resolve_toolset", a);
            if (sp4_raised(r)) { sp4_set_free(&out); return r; }
            sp4_set_from_arr(&out, r);
            json_free(r);
        }
    }
    return sp4_set_array(&out);
}
/* PoP: sp4_resolve_toolsets @ gateway/session_policy.py:_resolve_toolsets */
static json_t *sp4_resolve_toolsets(const json_t *params, json_t *config, const char *source,
        bool safe_mode, bool coding_posture, bool bypass, const json_t *builtin,
        const json_t *surface_toolsets, const sp4_seams_t *s) {
    const char *surface = (source && strcmp(source, "acp") == 0) ? "acp" : "cli";
    bool has_explicit = params && sp4_is_obj(params) && json_has((json_t *)params, "toolsets");
    const json_t *explicit = has_explicit ? sp4_oget(params, "toolsets") : NULL;
    sp4_set_t enabled; sp4_set_init(&enabled);
    if (has_explicit) {
        if (!sp4_is_arr(explicit)) {
            char m[160];
            snprintf(m, sizeof(m), "must be a list of toolset names, got %s", sp4_type_name(explicit));
            return sp4_refuse("toolsets", m);
        }
        if (!bypass) {
            json_t *a = sp4_args(); sp4_j(a, "config", config);
            json_t *res = sp4_call(s, "saved_toolset_resolver", a);
            if (sp4_raised(res)) return res;
            json_free(res);
        }
        const json_t *configured = sp4_oget(config, "mcp_servers");
        bool has_cfg = sp4_is_obj(configured) && configured->c.count > 0;
        sp4_set_t enabled_servers; sp4_set_init(&enabled_servers);
        if (has_cfg) {
            json_t *a = sp4_args(); sp4_j(a, "config", config);
            json_t *em = sp4_call(s, "enabled_mcp_server_names", a);
            if (sp4_raised(em)) { sp4_set_free(&enabled_servers); return em; }
            sp4_set_from_arr(&enabled_servers, em); json_free(em);
        }
        for (size_t i = 0; i < json_len(explicit); i++) {
            const json_t *nm = json_get(explicit, i);
            if (!sp4_is_str(nm)) return sp4_refuse_repr("toolsets", "unknown or invalid toolset name: ", nm);
            json_t *a = sp4_args();
            sp4_s(a, "name", sp4_sval(nm));
            sp4_s(a, "mode", bypass ? "builtin" : "saved");
            json_t *k = sp4_call(s, "known", a);
            if (sp4_raised(k)) { sp4_set_free(&enabled_servers); return k; }
            bool ok = sp4_truth(k); json_free(k);
            if (!ok) { sp4_set_free(&enabled_servers); return sp4_refuse_repr("toolsets", "unknown or invalid toolset name: ", nm); }
            const char *name = sp4_sval(nm);
            const char *server = name;
            if (strncmp(name, "mcp-", 4) == 0 && !(has_cfg && json_has((json_t *)configured, name))) server = name + 4;
            if (has_cfg && json_has((json_t *)configured, server) && !sp4_set_has(&enabled_servers, server)) {
                const json_t *ent = json_obj_get((json_t *)configured, server);
                const char *why = sp4_is_obj(ent) ? "is disabled in config" : "has no usable config entry";
                char m[200]; snprintf(m, sizeof(m), "MCP server '%s' %s", server, why);
                sp4_set_free(&enabled_servers);
                return sp4_refuse("toolsets", m);
            }
        }
        bool has_du = false;
        for (size_t i = 0; i < json_len(explicit); i++)
            if (sp4_is_str(json_get(explicit, i)) && sp4_streq(sp4_sval(json_get(explicit, i)), "desktop_ui")) has_du = true;
        if (has_du && !(source && strcmp(source, "gui") == 0)) {
            char m[160]; snprintf(m, sizeof(m), "desktop_ui is only available on the Desktop, not on %s", source ? source : "None");
            sp4_set_free(&enabled_servers);
            return sp4_refuse("toolsets", m);
        }
        sp4_set_free(&enabled_servers);
        json_t *pt = sp4_get_obj(config, "platform_toolsets");
        json_set(pt, surface, json_copy((json_t *)explicit));
    }
    if (!has_explicit && coding_posture) {
        json_t *a = sp4_args(); sp4_j(a, "config", config);
        json_t *f = sp4_call(s, "focus_toolsets", a);
        if (sp4_raised(f)) return f;
        sp4_set_from_arr(&enabled, f); json_free(f);
    } else {
        json_t *a = sp4_args(); sp4_j(a, "config", config); sp4_s(a, "surface", surface);
        json_t *p = sp4_call(s, "get_platform_tools", a);
        if (sp4_raised(p)) return p;
        sp4_set_from_arr(&enabled, p); json_free(p);
    }
    if (has_explicit) {
        json_t *exp = sp4_expand(explicit, true, s);
        if (sp4_raised(exp)) return exp;
        sp4_set_t requested; sp4_set_init(&requested); sp4_set_from_arr(&requested, exp); json_free(exp);
        json_t *u = sp4_unnamed_extension_toolsets(explicit, sp4_set_array(&requested), config, builtin, s);
        if (sp4_raised(u)) { sp4_set_free(&requested); return u; }
        sp4_set_t dropped; sp4_set_init(&dropped); sp4_set_from_arr(&dropped, u); json_free(u);
        sp4_set_remove(&enabled, &dropped);
        /* missing = requested - expand(enabled) */
        json_t *ea = sp4_set_array(&enabled);
        json_t *ex2 = sp4_expand(ea, true, s); json_free(ea);
        if (sp4_raised(ex2)) { sp4_set_free(&requested); sp4_set_free(&dropped); return ex2; }
        sp4_set_t miss; sp4_set_init(&miss); sp4_set_from_arr(&miss, ex2); json_free(ex2);
        sp4_set_remove(&miss, &requested);
        /* static = {n: resolve_toolset(n, False) for n in dropped & builtin} */
        sp4_set_t db2; sp4_set_init(&db2);
        { sp4_set_t bt; sp4_set_init(&bt); sp4_set_from_arr(&bt, builtin);
          sp4_set_intersect(&db2, &dropped, &bt); sp4_set_free(&bt); }
        for (size_t i = 0; i < db2.n; i++) {
            json_t *a = sp4_args(); sp4_s(a, "name", db2.v[i]); sp4_b(a, "include_registry", false);
            json_t *rt = sp4_call(s, "resolve_toolset", a);
            if (sp4_raised(rt)) { sp4_set_free(&requested); sp4_set_free(&dropped); sp4_set_free(&miss); sp4_set_free(&db2); return rt; }
            sp4_set_t tools; sp4_set_init(&tools); sp4_set_from_arr(&tools, rt); json_free(rt);
            if (sp4_set_has(&miss, db2.v[i]) || sp4_set_subset(&tools, &requested)) {
                /* tools & miss and tools <= requested -> add n back */
                sp4_set_t im; sp4_set_init(&im); sp4_set_intersect(&im, &tools, &miss);
                if (im.n > 0 && sp4_set_subset(&tools, &requested)) sp4_set_add(&enabled, db2.v[i]);
                sp4_set_free(&im);
            }
            sp4_set_free(&tools);
        }
        json_t *ea2 = sp4_set_array(&enabled);
        json_t *eff = sp4_expand(ea2, true, s); json_free(ea2);
        if (sp4_raised(eff)) { sp4_set_free(&requested); sp4_set_free(&dropped); sp4_set_free(&miss); sp4_set_free(&db2); return eff; }
        sp4_set_t effective; sp4_set_init(&effective); sp4_set_from_arr(&effective, eff); json_free(eff);
        sp4_set_t notavail; sp4_set_init(&notavail); sp4_set_sub(&notavail, &requested, &effective);
        if (notavail.n > 0) {
            sp4_set_sort(&notavail);
            char *joined = sp4_join_comma((const char **)notavail.v, notavail.n);
            char m[512]; snprintf(m, sizeof(m), "not available on this surface: %s", joined);
            free(joined);
            sp4_set_free(&notavail); sp4_set_free(&effective); sp4_set_free(&requested);
            sp4_set_free(&dropped); sp4_set_free(&miss); sp4_set_free(&db2);
            return sp4_refuse("toolsets", m);
        }
        sp4_set_free(&notavail); sp4_set_free(&effective); sp4_set_free(&requested);
        sp4_set_free(&dropped); sp4_set_free(&miss); sp4_set_free(&db2);
    }
    if (!has_explicit && source && (strcmp(source, "tui") == 0 || strcmp(source, "gui") == 0)) {
        const json_t *ag = sp4_oget(config, "agent");
        const json_t *dv = sp4_is_obj(ag) ? sp4_oget(ag, "disabled_toolsets") : NULL;
        json_t *a = sp4_args(); sp4_j(a, "value", dv);
        json_t *dis = sp4_call(s, "parse_config_string_list", a);
        if (sp4_raised(dis)) return dis;
        sp4_set_t disabled; sp4_set_init(&disabled); sp4_set_from_arr(&disabled, dis); json_free(dis);
        sp4_set_t surface_set; sp4_set_init(&surface_set);
        if (source && strcmp(source, "gui") == 0) sp4_set_from_arr(&surface_set, surface_toolsets);
        else sp4_set_add(&surface_set, "project");
        bool had_du = sp4_set_has(&surface_set, "desktop_ui");
        sp4_set_remove(&surface_set, &disabled);
        if (had_du) sp4_set_add(&surface_set, "desktop_ui");
        for (size_t i = 0; i < surface_set.n; i++) sp4_set_add(&enabled, surface_set.v[i]);
        sp4_set_free(&surface_set); sp4_set_free(&disabled);
    }
    if (safe_mode) {
        json_t *a = sp4_args();
        json_t *pk = sp4_call(s, "get_plugin_toolset_keys", a);
        if (sp4_raised(pk)) return pk;
        sp4_set_t plugins; sp4_set_init(&plugins); sp4_set_from_arr(&plugins, pk); json_free(pk);
        sp4_set_remove(&enabled, &plugins);
        sp4_set_free(&plugins);
    }
    sp4_last_enabled = sp4_set_array(&enabled);
    return json_copy(sp4_last_enabled);
}


/* -- 9. _apply_launch_overrides -- */
static bool sp4_bad_url(const char *value) {
    if (!value) return true;
    const char *p = strstr(value, "://");
    if (!p || p == value) return true;
    size_t sl = (size_t)(p - value);
    if (!((sl == 4 && strncmp(value, "http", 4) == 0) || (sl == 5 && strncmp(value, "https", 5) == 0))) return true;
    if (strchr(value, '?') || strchr(value, '#')) return true;
    const char *net = p + 3;
    const char *end = net + strcspn(net, "/?#");
    if (end == net) return true;                 /* no hostname */
    if (memchr(net, '@', (size_t)(end - net)) != NULL) return true; /* credentials */
    return false;
}
/* PoP: sp4_apply_launch_overrides @ gateway/session_policy.py:_apply_launch_overrides */
static json_t *sp4_apply_launch_overrides(const json_t *params, json_t *config, const sp4_seams_t *s) {
    json_t *a = sp4_args(); sp4_j(a, "params", params);
    json_t *pa = sp4_call(s, "without_auto_pick", a);
    if (sp4_raised(pa)) return pa;
    a = sp4_args(); sp4_j(a, "params", pa); sp4_j(a, "config", config);
    json_t *pp = sp4_call(s, "with_entry_pick", a);
    json_free(pa);
    if (sp4_raised(pp)) return pp;
    bool picks = false;
    const json_t *prov = sp4_oget(pp, "provider");
    if (sp4_is_obj(pp) && json_has((json_t *)pp, "provider")) {
        a = sp4_args(); sp4_j(a, "picked", prov); sp4_j(a, "config", config);
        json_t *k = sp4_call(s, "keeps_profile_route", a);
        if (sp4_raised(k)) { json_free(pp); return k; }
        picks = sp4_truth(k); json_free(k);
    }
    if (prov && !picks) {
        a = sp4_args();
        json_t *rbk = sp4_call(s, "route_bound_keys", a);
        if (sp4_raised(rbk)) { json_free(pp); return rbk; }
        json_t *model = sp4_get_obj(config, "model");
        if (sp4_is_arr(rbk)) for (size_t i = 0; i < json_len(rbk); i++) {
            const char *key = sp4_sval(json_get(rbk, i));
            if (key) json_obj_del(model, key);
        }
        json_free(rbk);
    }
    const char *route_keys[2] = { "provider", "base_url" };
    for (int i = 0; i < 2; i++) {
        const char *key = route_keys[i];
        if (!sp4_is_obj(pp) || !json_has((json_t *)pp, key)) continue;
        const json_t *value = sp4_oget(pp, key);
        if (!sp4_is_str(value)) { json_free(pp); return sp4_refuse(key, "must be a non-empty string"); }
        char *st = sp4_strip(sp4_sval(value));
        bool empty = st[0] == '\0'; free(st);
        if (empty) { json_free(pp); return sp4_refuse(key, "must be a non-empty string"); }
        if (strcmp(key, "base_url") == 0 && sp4_bad_url(sp4_sval(value))) {
            json_free(pp);
            return sp4_refuse("base_url", "must be an http(s) URL without credentials, query or fragment");
        }
        json_set(sp4_get_obj(config, "model"), key, json_copy((json_t *)value));
    }
    const char *flags[] = { "ignore_rules", "yolo", "checkpoints", "accept_hooks",
        "pass_session_id", "follow_profile_config", "cwd_explicit", "verbose" };
    for (size_t i = 0; i < sizeof(flags)/sizeof(flags[0]); i++)
        if (sp4_is_obj(pp) && json_has((json_t *)pp, flags[i]) && !sp4_is_bool(sp4_oget(pp, flags[i]))) {
            json_free(pp); return sp4_refuse(flags[i], "must be a boolean");
        }
    if (sp4_is_obj(pp) && json_has((json_t *)pp, "service_tier")) {
        const json_t *v = sp4_oget(pp, "service_tier");
        a = sp4_args(); sp4_j(a, "value", v);
        json_t *tier = sp4_call(s, "parse_exact_service_tier", a);
        if (sp4_raised(tier)) {
            if (sp4_caught(tier, "ValueError")) { json_free(tier); tier = json_null(); }
            else { json_free(pp); return tier; }
        }
        if (sp4_is_null(tier)) {
            json_t *r = sp4_refuse_repr("service_tier", "not a known tier: ", v);
            json_free(pp); return r;
        }
        json_set(sp4_get_obj(config, "agent"), "service_tier", json_copy(tier));
        json_free(tier);
    }
    if (sp4_is_obj(pp) && json_has((json_t *)pp, "run_budget")) {
        const json_t *v = sp4_oget(pp, "run_budget");
        if (sp4_is_bool(v) || !sp4_is_num(v) || v->num_val <= 0) {
            json_free(pp); return sp4_refuse("run_budget", "must be a positive number of seconds");
        }
        json_set(sp4_get_obj(config, "agent"), "run_budget_seconds", json_number(v->num_val));
    }
    if (sp4_is_obj(pp) && sp4_truth(sp4_oget(pp, "checkpoints"))) {
        const json_t *section = sp4_oget(config, "checkpoints");
        json_t *ns;
        if (sp4_is_obj(section)) ns = json_copy((json_t *)section);
        else { ns = json_object(); json_set(ns, "enabled", json_bool(true)); }
        json_set(ns, "enabled", json_bool(true));
        json_set(config, "checkpoints", ns);
    }
    if (sp4_is_obj(pp) && json_has((json_t *)pp, "max_turns")) {
        const json_t *v = sp4_oget(pp, "max_turns");
        json_t *limit = NULL;
        if (sp4_is_num(v) || sp4_is_str(v)) {
            a = sp4_args(); sp4_j(a, "value", v); json_set(a, "default", json_number(0));
            limit = sp4_call(s, "resolve_turn_limit", a);
            if (sp4_raised(limit)) { json_free(pp); return limit; }
        } else limit = json_number(0);
        if (!sp4_truth(limit)) {
            json_t *r = sp4_refuse_repr("max_turns", "unreadable cap: ", v);
            json_free(limit); json_free(pp); return r;
        }
        json_set(sp4_get_obj(config, "agent"), "max_turns", json_copy(limit));
        json_free(limit);
    }
    if (sp4_is_obj(pp) && json_has((json_t *)pp, "reasoning")) {
        const json_t *v = sp4_oget(pp, "reasoning");
        bool bad = !sp4_is_str(v);
        if (!bad) {
            a = sp4_args(); sp4_j(a, "value", v);
            json_t *eff = sp4_call(s, "parse_reasoning_effort", a);
            if (sp4_raised(eff)) { json_free(pp); return eff; }
            bad = sp4_is_null(eff); json_free(eff);
        }
        if (bad) {
            json_t *r = sp4_refuse_repr("reasoning", "not a known effort level: ", v);
            json_free(pp); return r;
        }
        json_t *agent = sp4_get_obj(config, "agent");
        json_set(agent, "reasoning_effort", json_copy((json_t *)v));
        json_obj_del(agent, "reasoning_overrides");
    }
    json_free(pp);
    return json_null();
}


/* -- 10. build_policy -- */
static json_t *sp4_sorted_copy(const json_t *v) {
    if (!v) return json_null();
    if (v->type == JSON_OBJECT) {
        json_t *out = json_object();
        size_t n = v->c.count;
        char **keys = malloc((n ? n : 1) * sizeof(char *));
        for (size_t i = 0; i < n; i++) keys[i] = strdup(v->c.keys[i]);
        for (size_t i = 1; i < n; i++) { char *t = keys[i]; size_t j = i;
            while (j > 0 && strcmp(keys[j-1], t) > 0) { keys[j] = keys[j-1]; j--; } keys[j] = t; }
        for (size_t i = 0; i < n; i++) {
            json_set(out, keys[i], sp4_sorted_copy(json_obj_get((json_t *)v, keys[i])));
            free(keys[i]);
        }
        free(keys); return out;
    }
    if (v->type == JSON_ARRAY) {
        json_t *out = json_array();
        for (size_t i = 0; i < json_len(v); i++) json_append(out, sp4_sorted_copy(json_get(v, i)));
        return out;
    }
    return json_copy((json_t *)v);
}
static char *sp4_serialize_sorted(const json_t *v) {
    json_t *s = sp4_sorted_copy(v);
    char *out = json_serialize(s);
    json_free(s);
    return out ? out : strdup("null");
}
static bool sp4_is_false(const json_t *v) { return sp4_is_bool(v) && !v->bool_val; }
/* Replace a caller-owned object's contents in place (never free the object:
 * the harness retains ownership of in.config for out.state). */
static void sp4_obj_replace(json_t *dst, const json_t *src) {
    if (!dst || dst->type != JSON_OBJECT) return;
    for (size_t i = 0; i < dst->c.count; i++) { free(dst->c.keys[i]); json_free(dst->c.items[i]); }
    free(dst->c.keys); free(dst->c.items);
    dst->c.keys = NULL; dst->c.items = NULL; dst->c.count = 0;
    if (src && src->type == JSON_OBJECT)
        for (size_t i = 0; i < src->c.count; i++)
            json_set(dst, src->c.keys[i], json_copy(src->c.items[i]));
}
static bool sp4_is_true(const json_t *v) { return sp4_is_bool(v) && v->bool_val; }

/* PoP: sp4_build_policy @ gateway/session_policy.py:build_policy */
static json_t *sp4_build_policy(const json_t *params, json_t *config, const json_t *priv,
        bool profile_terminal, bool workspace_posture, bool client_local, const char *home,
        const json_t *builtin, const json_t *surface_toolsets, const sp4_seams_t *s) {
    bool has_params = sp4_is_obj(params);
    json_t *source_val = (has_params && json_has((json_t *)params, "source"))
                       ? (json_t *)sp4_oget(params, "source") : json_string("cli");
    const char *source = sp4_sval(source_val);
    if (source && strcmp(source, "a2a") == 0) {
        json_t *a = sp4_args(); sp4_j(a, "params", params); sp4_j(a, "config", config); sp4_j(a, "private_secrets", priv);
        return sp4_call(s, "build_forward_policy", a);
    }
    /* unknown = sorted(set(params) - CREATE_FIELDS) */
    size_t nk = has_params ? params->c.count : 0;
    const char **unknown = malloc((nk ? nk : 1) * sizeof(char *));
    size_t nu = 0;
    for (size_t i = 0; i < nk; i++) if (!sp4_is_create_field(params->c.keys[i])) unknown[nu++] = params->c.keys[i];
    for (size_t i = 1; i < nu; i++) { const char *t = unknown[i]; size_t j = i;
        while (j > 0 && strcmp(unknown[j-1], t) > 0) { unknown[j] = unknown[j-1]; j--; } unknown[j] = t; }
    if (nu > 0 || !sp4_is_str(source_val) || !sp4_surface(source)) {
        json_t *r;
        if (nu > 0) {
            size_t fl = strlen(unknown[0]) + 8;
            char *field = malloc(fl); snprintf(field, fl, "params.%s", unknown[0]);
            r = sp4_refuse(field, "unknown create field"); free(field);
        } else {
            json_t *rep = sp4_repr(source_val);
            char *rp = strdup(sp4_sval(rep) ? sp4_sval(rep) : "None");
            json_free(rep);
            char *msg = malloc(strlen(rp) + 16); snprintf(msg, strlen(rp)+16, "not a surface: %s", rp);
            r = sp4_refuse("source", msg); free(msg); free(rp);
        }
        free(unknown); return r;
    }
    free(unknown);
    const char *bypass_names[2] = { "safe_mode", "ignore_user_config" };
    for (int i = 0; i < 2; i++) {
        const char *nm = bypass_names[i];
        if (has_params && json_has((json_t *)params, nm) && !sp4_is_bool(sp4_oget(params, nm)))
            return sp4_refuse(nm, "must be a boolean");
    }
    bool safe_mode = has_params && json_has((json_t *)params, "safe_mode") ? sp4_oget(params,"safe_mode")->bool_val : false;
    bool ignore_user = safe_mode || (has_params && json_has((json_t *)params, "ignore_user_config")
                                     && sp4_oget(params,"ignore_user_config")->bool_val);
    if (has_params && json_has((json_t *)params, "api_key")) {
        const json_t *v = sp4_oget(params, "api_key");
        if (!sp4_is_str(v)) return sp4_refuse("api_key", "must be a non-empty string");
        char *st = sp4_strip(sp4_sval(v)); bool empty = st[0] == '\0'; free(st);
        if (empty) return sp4_refuse("api_key", "must be a non-empty string");
    }
    const json_t *model = has_params ? sp4_oget(params, "model") : NULL;
    if (has_params && json_has((json_t *)params, "model")) {
        if (!sp4_is_str(model)) return sp4_refuse("model", "must be a non-empty string");
        char *st = sp4_strip(sp4_sval(model)); bool empty = st[0] == '\0'; free(st);
        if (empty) return sp4_refuse("model", "must be a non-empty string");
    }
    char *cwd = NULL;
    {
        json_t *a = sp4_args();
        json_t *rc = sp4_call(s, "resolve_agent_cwd", a);
        if (sp4_raised(rc)) return rc;
        if (has_params && json_has((json_t *)params, "cwd")) {
            const json_t *v = sp4_oget(params, "cwd");
            cwd = sp4_is_str(v) ? strdup(sp4_sval(v)) : sp4_pystr(v);
        } else cwd = sp4_pystr(rc);
        json_free(rc);
    }
    bool cwd_ok = sp4_is_abs(cwd);
    if (cwd_ok) {
        json_t *a = sp4_args(); sp4_s(a, "path", cwd);
        json_t *d = sp4_call(s, "os.path.isdir", a);
        if (sp4_raised(d)) { free(cwd); return d; }
        cwd_ok = sp4_truth(d); json_free(d);
    }
    if (!cwd_ok) { free(cwd); return sp4_refuse("cwd", "must be an absolute path to an existing directory"); }
    if (has_params && json_has((json_t *)params, "row_cwd")) {
        const json_t *v = sp4_oget(params, "row_cwd");
        bool bad = !sp4_is_str(v);
        if (!bad) { char *st = sp4_strip(sp4_sval(v)); bad = st[0] != '\0' && !sp4_is_abs(st); free(st); }
        if (bad) { free(cwd); return sp4_refuse("row_cwd", "must be an absolute path when set"); }
    }
    if (has_params && json_has((json_t *)params, "cwd_explicit") && sp4_is_false(sp4_oget(params,"cwd_explicit"))
        && json_has((json_t *)params, "cwd")) {
        json_t *pc = sp4_profile_workspace_cwd(home, s);
        if (sp4_raised(pc)) { free(cwd); return pc; }
        if (sp4_truth(pc)) { free(cwd); cwd = strdup(sp4_sval(pc) ? sp4_sval(pc) : ""); }
        json_free(pc);
    }
    const json_t *editor = has_params ? sp4_oget(params, "editor") : NULL;
    {
        json_t *a = sp4_args(); sp4_s(a, "source", source); sp4_j(a, "editor", editor);
        json_t *v = sp4_call(s, "validate_editor", a);
        if (sp4_raised(v)) { free(cwd); return v; }
        json_free(v);
    }
    if (has_params && json_has((json_t *)params, "client")) {
        json_t *a = sp4_args(); sp4_j(a, "client", sp4_oget(params, "client"));
        json_t *prob = sp4_call(s, "client_problem", a);
        if (sp4_raised(prob)) { free(cwd); return prob; }
        if (sp4_truth(prob)) {
            char *p = sp4_pystr(prob);
            json_t *r = sp4_refuse("client", p); free(p); json_free(prob); free(cwd); return r;
        }
        json_free(prob);
    }
    {
        json_t *a = sp4_args(); sp4_j(a, "params", params);
        json_t *v = sp4_call(s, "launch_skills", a);
        if (sp4_raised(v)) { free(cwd); return v; }
        json_free(v);
    }
    json_t *orig_config = config;   /* caller-owned: emitted as out.state.config */
    /* LIVE: config = present_sections(json.loads(json.dumps(config))) — the
     * rest of build_policy mutates a private working copy, so the caller keeps
     * its original config on every exception path. On success the final
     * working state is copied back below (matching LIVE's pol.config_json). */
    config = config ? json_copy(config) : json_object();
    {
        json_t *cp = json_copy(config);
        json_t *a = sp4_args(); sp4_j(a, "config", cp); json_free(cp);
        json_t *ps = sp4_call(s, "present_sections", a);
        if (sp4_raised(ps)) { free(cwd); return ps; }
        sp4_obj_replace(config, ps);
        json_free(ps);
    }
    json_t *pa = NULL;
    {
        json_t *a = sp4_args(); sp4_j(a, "params", params);
        pa = sp4_call(s, "without_auto_pick", a);
        if (sp4_raised(pa)) { free(cwd); return pa; }
    }
    bool picked = false;
    if (sp4_is_obj(pa)) {
        const char *pick_keys[] = {"model","provider","base_url"};
        for (int i = 0; i < 3; i++) if (json_has((json_t *)pa, pick_keys[i])) picked = true;
    }
    if (has_params && json_has((json_t *)params, "follow_profile_config")
        && sp4_is_true(sp4_oget(params,"follow_profile_config")) && picked) {
        json_t *a = sp4_args(); sp4_j(a, "config", config);
        json_t *tgt = sp4_call(s, "profile_target", a);
        if (sp4_raised(tgt)) { json_free(pa); free(cwd); return tgt; }
        json_set(sp4_get_obj(config, "model"), "selection_profile", sp4_is_arr(tgt) ? json_copy(tgt) : json_array());
        json_free(tgt);
    }
    json_free(pa);
    {
        json_t *r = sp4_apply_launch_overrides(params, config, s);
        if (sp4_raised(r)) { free(cwd); return r; }
        json_free(r);
    }
    bool named = workspace_posture && strcmp(source, "tool") != 0
              && has_params && json_has((json_t *)params, "cwd") && !json_has((json_t *)params, "toolsets");
    bool coding = false;
    if (named) {
        json_t *a = sp4_args(); sp4_j(a, "config", config); sp4_s(a, "source", source); sp4_s(a, "cwd", cwd);
        json_t *v = sp4_call(s, "_in_coding_workspace", a);
        if (sp4_raised(v)) { free(cwd); return v; }
        coding = sp4_truth(v); json_free(v);
    }
    sp4_last_enabled = NULL;
    {
        json_t *r = sp4_resolve_toolsets(params, config, source, safe_mode, coding, ignore_user,
                                         builtin, surface_toolsets, s);
        if (sp4_raised(r)) { free(cwd); return r; }
        json_free(r);
    }
    json_t *terminal = NULL;
    {
        json_t *a = sp4_args();
        if (profile_terminal) sp4_s(a, "home", home);
        terminal = sp4_call(s, profile_terminal ? "build_profile_terminal_scope" : "default_terminal_scope", a);
        if (sp4_raised(terminal)) { free(cwd); return terminal; }
        if (!sp4_is_obj(terminal)) { json_free(terminal); terminal = json_object(); }
    }
    bool force_cwd = has_params && json_has((json_t *)params, "cwd_explicit") && sp4_is_true(sp4_oget(params,"cwd_explicit"));
    if (!force_cwd) {
        json_t *a = sp4_args(); sp4_j(a, "terminal", terminal);
        json_t *v = sp4_call(s, "terminal_uses_host_cwd", a);
        if (sp4_raised(v)) { json_free(terminal); free(cwd); return v; }
        force_cwd = sp4_truth(v); json_free(v);
    }
    if (force_cwd) json_set(terminal, "TERMINAL_CWD", json_string(cwd));
    char *client_env_json = NULL, *rules_json = NULL;
    bool client_env_remote = false;
    if (has_params && json_has((json_t *)params, "client_env")) {
        const json_t *ce = sp4_oget(params, "client_env");
        json_t *a = sp4_args(); sp4_j(a, "config", config); sp4_b(a, "local", client_local);
        json_t *rules = sp4_call(s, "client_env_rules", a);
        if (sp4_raised(rules)) { json_free(terminal); free(cwd); return rules; }
        a = sp4_args(); sp4_j(a, "client_env", ce); sp4_j(a, "rules", rules);
        json_t *norm = sp4_call(s, "normalise_client_env", a);
        if (sp4_raised(norm)) {
            if (sp4_caught(norm, "ValueError")) {
                const json_t *m = json_obj_get(norm, "msg");
                char *msg = sp4_pystr(m);
                json_free(norm); json_free(rules); json_free(terminal); free(cwd);
                json_t *r = sp4_refuse("client_env", msg); free(msg); return r;
            }
            json_free(rules); json_free(terminal); free(cwd); return norm;
        }
        client_env_json = sp4_serialize_sorted(norm);
        rules_json = sp4_serialize_sorted(rules);
        client_env_remote = !client_local;
        json_free(norm); json_free(rules);
    }
    json_t *request = json_object();
    if (has_params) {
        for (size_t i = 0; i < params->c.count; i++) {
            const char *k = params->c.keys[i];
            if (strcmp(k,"request_id")==0 || strcmp(k,"api_key")==0 || strcmp(k,"client_env")==0) continue;
            json_set(request, k, json_copy(params->c.items[i]));
        }
    }
    if (!json_has(request, "source")) json_set(request, "source", json_string("cli"));
    {
        json_t *a = sp4_args(); sp4_j(a, "request", request); sp4_j(a, "private_secrets", priv);
        json_t *v = sp4_call(s, "private_editor_request", a);
        if (sp4_raised(v)) { json_free(request); json_free(terminal); free(cwd); free(client_env_json); free(rules_json); return v; }
        if (sp4_is_obj(v)) { json_free(request); request = v; } else json_free(v);
    }
    {
        json_t *a = sp4_args(); sp4_j(a, "value", config); sp4_j(a, "private", priv);
        json_t *v = sp4_call(s, "extract_config_secrets_config", a);
        if (sp4_raised(v)) { json_free(request); json_free(terminal); free(cwd); free(client_env_json); free(rules_json); return v; }
        if (sp4_is_obj(v)) { json_free(config); config = v; } else json_free(v);
    }
    {
        json_t *a = sp4_args(); sp4_j(a, "value", terminal); sp4_j(a, "private", priv); sp4_b(a, "tuple_path", true);
        json_t *v = sp4_call(s, "extract_config_secrets_terminal", a);
        if (sp4_raised(v)) { json_free(request); json_free(terminal); free(cwd); free(client_env_json); free(rules_json); return v; }
        if (sp4_is_obj(v)) { json_free(terminal); terminal = v; } else json_free(v);
    }
    json_t *out = json_object();
    json_set(out, "source", json_string(source));
    const char *plat = sp4_surface(source);
    json_set(out, "platform", plat ? json_string(plat) : json_null());
    json_set(out, "cwd", json_string(cwd));
    json_set(out, "model", model ? json_copy((json_t *)model) : json_null());
    json_t *ets = sp4_last_enabled ? sp4_last_enabled : json_array();
    sp4_last_enabled = NULL;
    json_set(out, "toolsets", ets);
    char *cfg_s = json_serialize(config);
    char *req_s = sp4_serialize_sorted(request);
    char *term_s = json_serialize(terminal);
    json_set(out, "config_json", json_string(cfg_s ? cfg_s : "{}"));
    json_set(out, "request_json", json_string(req_s ? req_s : "{}"));
    json_set(out, "terminal_json", json_string(term_s ? term_s : "{}"));
    free(cfg_s); free(req_s); free(term_s);
    json_set(out, "credential_ref", json_null());
    json_set(out, "config_secret_ref", json_null());
    json_set(out, "editor_mcp_json", json_null());
    json_set(out, "kanban_json", json_null());
    json_set(out, "safe_mode", json_bool(safe_mode));
    json_set(out, "ignore_user_config", json_bool(ignore_user));
    json_set(out, "skills_prompt", json_null());
    json_set(out, "coding_posture", json_bool(coding));
    json_set(out, "client_env_json", client_env_json ? json_string(client_env_json) : json_null());
    json_set(out, "client_env_remote", json_bool(client_env_remote && client_env_json != NULL));
    json_set(out, "client_env_rules_json", rules_json ? json_string(rules_json) : json_null());
    json_set(out, "edit_approval_policy", json_string("ask"));
    free(client_env_json); free(rules_json);
    json_free(request); json_free(terminal); free(cwd);
    /* Success: reflect the final working config into the caller's object so
     * out.state.config matches LIVE's pol.config_json; then release our copy. */
    sp4_obj_replace(orig_config, config);
    json_free(config);
    return out;
}
