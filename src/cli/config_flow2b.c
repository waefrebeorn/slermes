/* config_flow2b.c — FLOW-core port of the second-wave UNCLAIMED remainder of
 * hermes_cli/config.py (the three platform-plugin manifest generators, the
 * .env read/write/lock helpers, the two config CLI report commands, and the
 * migration offer/policy helpers).
 *
 * Sibling of config_flow2.c: same universal seam dispatcher model. Every
 * collaborator / I/O call goes through cfg2b_seam(io, name, args); the harness
 * dispatcher appends `name` to io->trace, returns the scripted result, applies
 * "state" effects, and models a raised exception as
 * {"__raise__": {type,message,...}}. An "import_raise" entry models a failed
 * in-function import; the dispatcher marks it "__untrace__" and cfg2b_seam
 * retracts the trace token while KEEPING the flag, so a consumer can tell an
 * Exception import failure (caught by `except Exception`) from a call raise.
 *
 * Two cosmetic seams are AUTO (no script, never fail closed): "print"
 * (captures args.text into cap_stdout/cap_stderr per args.file) and "color"
 * (returns args.text verbatim — the identity / NO_COLOR projection). Every
 * other seam is SCRIPTED and FAIL-CLOSED: a missing script raises
 * RuntimeError("unscripted seam: <name>").
 *
 * Fixture input domain is ASCII; the few non-ASCII glyphs in message
 * CONSTANTS (check/cross/warn/bullet/em-dash) are byte-pinned UTF-8 literals.
 * Paths are posix strings; os.stat_result doubles project to
 * {"__stat__": {"st_mode": N}} and exception doubles to {"__raise__": spec}.
 * Generators materialize as JSON arrays of tuples (no lazy semantics).
 *
 * Ownership: seam results are caller-owned (json_free). io->trace / io->state
 * / cap_stdout / cap_stderr are harness-owned. cfg2b_seam() frees its args.
 * Every public cfg2b_* function returns a NEW reference (or a raise sentinel).
 *
 * CODE-ONLY: this file is included by tests/t_port_config_flow2b.c and is not
 * registered in build/objects.mk (scanner credit is deliberately not chased).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include "hermes_json.h"

/* ── message-constant glyphs (byte-pinned UTF-8) ───────────────────────── */
#define CFG2B_CROSS  "\xe2\x9c\x97"   /* ✗ */
#define CFG2B_CHECK  "\xe2\x9c\x93"   /* ✓ */
#define CFG2B_WARN   "\xe2\x9a\xa0"   /* ⚠ */
#define CFG2B_BULLET "\xe2\x80\xa2"   /* • */
#define CFG2B_EMDASH "\xe2\x80\x94"  /* — */

#define CFG2B_RESET  "\x1b[0m"
#define CFG2B_BOLD   "\x1b[1m"
#define CFG2B_DIM    "\x1b[2m"
#define CFG2B_RED    "\x1b[31m"
#define CFG2B_GREEN  "\x1b[32m"
#define CFG2B_YELLOW "\x1b[33m"

/* ── seam dispatcher + trace + state ───────────────────────────────────── */
typedef struct cfg2b_flow_io_t {
    json_t *(*call)(struct cfg2b_flow_io_t *io, const char *name, json_t *args);
    json_t *trace;          /* ARRAY of string tokens (harness-owned) */
    json_t *state;          /* OBJECT (harness-owned) */
    json_t *cap_stdout;
    json_t *cap_stderr;
    void   *ctx;
} cfg2b_flow_io_t;

static void cfg2b_trace(cfg2b_flow_io_t *io, const char *token) {
    if (!io || !io->trace || io->trace->type != JSON_ARRAY) return;
    json_append(io->trace, json_string(token));
}
static json_t *cfg2b_call(cfg2b_flow_io_t *io, const char *name, json_t *args) {
    if (io && io->call) return io->call(io, name, args);
    return json_null();
}
static bool cfg2b_raised(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}
static json_t *cfg2b_raise_spec(const json_t *r) {
    return cfg2b_raised(r) ? json_obj_get(r, "__raise__") : NULL;
}
static json_t *cfg2b_make_raise(const char *type, const char *message) {
    json_t *spec = json_object();
    json_set(spec, "type", json_string(type));
    json_set(spec, "message", json_string(message ? message : ""));
    json_t *r = json_object();
    json_set(r, "__raise__", spec);
    return r;
}
static json_t *cfg2b_make_raise_code(const char *type, double code) {
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
/* seam: trace, dispatch, free args. An "__untrace__" result (failed import)
 * retracts the trace token but keeps the flag. */
static json_t *cfg2b_seam(cfg2b_flow_io_t *io, const char *name, json_t *args) {
    cfg2b_trace(io, name);
    json_t *r = cfg2b_call(io, name, args);
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
static bool cfg2b_truthy(const json_t *v) {
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
static bool cfg2b_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool cfg2b_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static const char *cfg2b_sval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
/* value-node accessors (json_get_num/bool take an OBJECT + key; these take a
 * JSON value node directly, as returned by json_get / json_obj_get). */
static double cfg2b_val_num(const json_t *v, double d) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : d;
}
static bool cfg2b_val_bool(const json_t *v, bool d) {
    return (v && v->type == JSON_BOOL) ? v->bool_val : d;
}
static char *cfg2b_join(const char *a, const char *b) {
    size_t na = strlen(a ? a : ""), nb = strlen(b ? b : "");
    char *out = malloc(na + nb + 1);
    if (!out) return NULL;
    if (na) memcpy(out, a, na);
    if (nb) memcpy(out + na, b, nb);
    out[na + nb] = '\0';
    return out;
}
/* Path-style join mirroring FakePath.__truediv__ (base.rstrip('/'), single '/'). */
static char *cfg2b_path_join(const char *base, const char *name) {
    const char *b = base ? base : "";
    size_t nb = strlen(b);
    if (nb == 1 && b[0] == '/') nb = 0;
    else while (nb > 0 && b[nb - 1] == '/') nb--;
    const char *nm = name ? name : "";
    size_t nn = strlen(nm);
    char *out = malloc(nb + 1 + nn + 1);
    if (!out) return NULL;
    if (nb) memcpy(out, b, nb);
    out[nb] = '/';
    if (nn) memcpy(out + nb + 1, nm, nn);
    out[nb + 1 + nn] = '\0';
    return out;
}
/* ASCII str.strip() (fixture domain is ASCII for user-supplied strings). */
static char *cfg2b_strip(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    size_t a = 0, b = n;
    while (a < b && isspace((unsigned char)p[a])) a++;
    while (b > a && isspace((unsigned char)p[b - 1])) b--;
    char *out = malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, p + a, b - a);
    out[b - a] = '\0';
    return out;
}
/* NEW sorted copy of a string array (Python sorted(set(names))). */
static json_t *cfg2b_sorted_unique_strs(const json_t *arr) {
    json_t *out = json_array();
    size_t n = (arr && arr->type == JSON_ARRAY) ? json_len(arr) : 0;
    if (n == 0) return out;
    const char **tmp = malloc(n * sizeof(char *));
    if (!tmp) return out;
    size_t m = 0;
    for (size_t i = 0; i < n; i++) {
        const char *v = cfg2b_sval(json_get(arr, i));
        bool dup = false;
        for (size_t j = 0; j < m; j++) if (strcmp(tmp[j], v) == 0) { dup = true; break; }
        if (!dup) tmp[m++] = v;
    }
    for (size_t i = 1; i < m; i++) {
        const char *v = tmp[i]; size_t j = i;
        while (j > 0 && strcmp(tmp[j - 1], v) > 0) { tmp[j] = tmp[j - 1]; j--; }
        tmp[j] = v;
    }
    for (size_t i = 0; i < m; i++) json_append(out, json_string(tmp[i]));
    free(tmp);
    return out;
}
static json_t *cfg2b_state_get(cfg2b_flow_io_t *io, const char *key) {
    if (!io || !io->state || io->state->type != JSON_OBJECT) return NULL;
    return json_obj_get(io->state, key);
}
/* get a value from io->state["results"] by key, creating {key: []} on demand. */
static json_t *cfg2b_results_list(cfg2b_flow_io_t *io, const char *key) {
    json_t *res = cfg2b_state_get(io, "results");
    if (!res || res->type != JSON_OBJECT) return json_array();
    json_t *lst = json_obj_get(res, key);
    if (!lst || lst->type != JSON_ARRAY) return json_array();
    return lst; /* borrowed: caller appends a copy into the live array */
}

/* ── full builtin exception hierarchy (for `except Exception` family tests) ─ */
typedef struct { const char *name; const char *parent; } cfg2b_exc_t;
static const cfg2b_exc_t CFG2B_EXC_TABLE[] = {
    {"BaseException", NULL},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"Exception", "BaseException"},
    {"TypeError", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"ArithmeticError", "Exception"},
    {"FloatingPointError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ImportError", "Exception"},
    {"ModuleNotFoundError", "ImportError"},
    {"LookupError", "Exception"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"MemoryError", "Exception"},
    {"NameError", "Exception"},
    {"UnboundLocalError", "NameError"},
    {"OSError", "Exception"},
    {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"ConnectionError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"TimeoutError", "OSError"},
    {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"SyntaxError", "Exception"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"SystemError", "Exception"},
    {"ValueError", "Exception"},
    {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"Warning", "Exception"},
    {"DeprecationWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"UserWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"UnicodeWarning", "Warning"},
    {"BytesWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {NULL, NULL},
};
static const char *cfg2b_exc_parent(const char *type) {
    if (!type) return NULL;
    for (int i = 0; CFG2B_EXC_TABLE[i].name; i++)
        if (strcmp(type, CFG2B_EXC_TABLE[i].name) == 0) return CFG2B_EXC_TABLE[i].parent;
    return NULL;
}
/* isinstance(exc, Exception): walk parent chain to "Exception". */
static bool cfg2b_exc_is_exception(const char *type) {
    const char *cur = type;
    for (int depth = 0; cur && depth < 32; depth++) {
        if (strcmp(cur, "Exception") == 0) return true;
        if (strcmp(cur, "BaseException") == 0) return false;
        cur = cfg2b_exc_parent(cur);
    }
    return false;
}

/* cosmetic AUTO seams. */
static json_t *cfg2b_print(cfg2b_flow_io_t *io, const char *text, bool to_stderr) {
    json_t *args = json_object();
    json_set(args, "text", json_string(text ? text : ""));
    json_set(args, "file", to_stderr ? json_string("stderr") : json_null());
    return cfg2b_seam(io, "print", args);
}
static json_t *cfg2b_color(cfg2b_flow_io_t *io, const char *text, const char *code) {
    json_t *args = json_object();
    json_set(args, "text", json_string(text ? text : ""));
    json_set(args, "codes", code ? json_array() : json_array());
    if (code) json_append(json_obj_get(args, "codes"), json_string(code));
    return cfg2b_seam(io, "color", args);
}
/* print(color(...)) — the dominant CLI pattern: color is AUTO, its result is
 * then handed to the AUTO print seam. */
static void cfg2b_cp(cfg2b_flow_io_t *io, const char *text, const char *code) {
    json_t *p = cfg2b_color(io, text, code);
    if (cfg2b_raised(p)) return;
    json_t *q = cfg2b_print(io, cfg2b_sval(p), false);
    json_free(q);
    json_free(p);
}

/* forward declarations (composed siblings). */
static json_t *cfg2b_platform_manifest_paths(cfg2b_flow_io_t *io, const char *home, const char *source);

/* ====================================================================
 * 1. _warn_invalid_platform_toolsets  (config.py:1262)
 * =================================================================== */
/* PoP: cfg2b_warn_invalid_platform_toolsets @ hermes_cli/config.py:_warn_invalid_platform_toolsets */
static json_t *cfg2b_warn_invalid_platform_toolsets(cfg2b_flow_io_t *io, bool quiet) {
    json_t *last = NULL;
    json_t *cfg = cfg2b_seam(io, "read_raw_config", NULL);
    if (cfg2b_raised(cfg)) { last = cfg; goto handler; }
    json_t *toolsets = json_obj_get(cfg, "platform_toolsets");
    json_t *ra = json_object(); json_set(ra, "config", json_copy(cfg));
    json_t *resolver = cfg2b_seam(io, "saved_toolset_resolver", ra);
    if (cfg2b_raised(resolver)) { json_free(cfg); last = resolver; goto handler; }
    json_t *va = json_object();
    json_set(va, "toolsets", toolsets ? json_copy(toolsets) : json_null());
    json_set(va, "resolver", json_copy(resolver));
    json_free(resolver);
    json_t *warnings = cfg2b_seam(io, "validate_platform_toolsets", va);
    json_free(cfg);
    if (cfg2b_raised(warnings)) { last = warnings; goto handler; }
    size_t n = (warnings && warnings->type == JSON_ARRAY) ? json_len(warnings) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *w = json_get(warnings, i);
        json_t *live = cfg2b_results_list(io, "warnings");
        if (live && live->type == JSON_ARRAY) json_append(live, json_copy(w));
        if (!quiet) {
            char *m1 = cfg2b_join("  ", CFG2B_WARN " ");
            char *m2 = cfg2b_join(m1, cfg2b_sval(w));
            free(m1);
            json_t *pr = cfg2b_print(io, m2, false);
            free(m2);
            if (cfg2b_raised(pr)) { last = pr; json_free(warnings); goto handler; }
            json_free(pr);
        }
    }
    json_free(warnings);
    return json_null();

handler: {
    json_t *spec = cfg2b_raise_spec(last);
    bool is_exc = cfg2b_exc_is_exception(spec ? json_get_str(spec, "type", "") : "");
    if (is_exc) {
        json_t *a = json_object();
        json_set(a, "fmt", json_string("platform_toolsets validation skipped: %s"));
        json_t *args = json_array();
        json_append(args, json_string(spec ? json_get_str(spec, "message", "") : ""));
        json_set(a, "args", args);
        json_t *d = cfg2b_seam(io, "logger.debug", a);
        json_free(d);
        json_free(last);
        return json_null();
    }
    return last; /* BaseException propagates */
}
}

/* ====================================================================
 * 2. _offer_new_optional_env_vars  (config.py:1290)
 * =================================================================== */
/* PoP: cfg2b_offer_new_optional_env_vars @ hermes_cli/config.py:_offer_new_optional_env_vars */
static json_t *cfg2b_offer_new_optional_env_vars(cfg2b_flow_io_t *io, double current_ver, double latest_ver,
                                          json_t *constants) {
    json_t *byver = json_obj_get(constants, "env_vars_by_version");
    json_t *optional = json_obj_get(constants, "optional_env_vars");
    json_t *names = json_array();
    for (double v = current_ver + 1; v <= latest_ver; v += 1.0) {
        char key[32]; snprintf(key, sizeof key, "%.0f", v);
        json_t *lst = json_obj_get(byver, key);
        size_t n = (lst && lst->type == JSON_ARRAY) ? json_len(lst) : 0;
        for (size_t i = 0; i < n; i++) {
            const char *nm = cfg2b_sval(json_get(lst, i));
            json_append(names, json_string(nm));
        }
    }
    json_t *sorted = cfg2b_sorted_unique_strs(names);
    json_free(names);
    /* new_and_unset = [(name,info) for name in sorted if not get_env_value(name)
     *                   and name in OPTIONAL_ENV_VARS] */
    json_t *pairs = json_array();
    size_t sn = json_len(sorted);
    for (size_t i = 0; i < sn; i++) {
        const char *nm = cfg2b_sval(json_get(sorted, i));
        json_t *info = json_obj_get(optional, nm);
        if (!info || info->type != JSON_OBJECT) continue;
        json_t *a = json_object(); json_set(a, "name", json_string(nm));
        json_t *ev = cfg2b_seam(io, "get_env_value", a);
        if (cfg2b_raised(ev)) { json_free(sorted); return ev; }
        bool unset = !cfg2b_truthy(ev);
        json_free(ev);
        if (!unset) continue;
        json_t *pair = json_array();
        json_append(pair, json_string(nm));
        json_append(pair, json_copy(info));
        json_append(pairs, pair);
    }
    json_free(sorted);
    size_t pn = json_len(pairs);
    if (pn == 0) { json_free(pairs); return json_null(); }
    /* heading */
    char cnt[32]; snprintf(cnt, sizeof cnt, "%zu", pn);
    char *h1 = cfg2b_join("\n  ", cnt);
    char *heading = cfg2b_join(h1, " new optional key(s) in this update:");
    free(h1);
    json_t *items = json_array();
    for (size_t i = 0; i < pn; i++) {
        json_t *pair = json_get(pairs, i);
        const char *nm = cfg2b_sval(json_get(pair, 0));
        json_t *info = json_get(pair, 1);
        json_t *desc = json_obj_get(info, "description");
        char *line = cfg2b_join(nm, CFG2B_EMDASH " ");
        char *line2 = cfg2b_join(line, desc ? cfg2b_sval(desc) : "");
        free(line);
        json_append(items, json_string(line2));
        free(line2);
    }
    json_t *oa = json_object();
    json_set(oa, "heading", json_string(heading));
    json_set(oa, "items", items);
    json_set(oa, "question", json_string("  Configure new keys? [y/N]: "));
    free(heading);
    json_t *accepted = cfg2b_seam(io, "_offer_list", oa);
    if (cfg2b_raised(accepted)) { json_free(pairs); return accepted; }
    bool yes = cfg2b_truthy(accepted);
    json_free(accepted);
    if (!yes) { json_free(pairs); return json_null(); }
    for (size_t i = 0; i < pn; i++) {
        json_t *pair = json_get(pairs, i);
        const char *nm = cfg2b_sval(json_get(pair, 0));
        json_t *info = json_get(pair, 1);
        json_t *desc = json_obj_get(info, "description");
        char *d1 = cfg2b_join("  ", desc ? cfg2b_sval(desc) : nm);
        json_t *pr = cfg2b_print(io, d1, false); free(d1);
        if (cfg2b_raised(pr)) { json_free(pairs); return pr; }
        json_free(pr);
        json_t *url = json_obj_get(info, "url");
        if (cfg2b_truthy(url)) {
            char *u1 = cfg2b_join("  Get your key at: ", cfg2b_sval(url));
            json_t *p2 = cfg2b_print(io, u1, false); free(u1);
            if (cfg2b_raised(p2)) { json_free(pairs); return p2; }
            json_free(p2);
        }
        json_t *prompt = json_obj_get(info, "prompt");
        char *p1 = cfg2b_join("  ", prompt && cfg2b_truthy(prompt) ? cfg2b_sval(prompt) : nm);
        char *p2 = cfg2b_join(p1, " (Enter to skip): "); free(p1);
        json_t *pa = json_object();
        json_set(pa, "name", json_string(nm));
        json_set(pa, "info", json_copy(info));
        json_set(pa, "prompt", json_string(p2));
        free(p2);
        json_t *res = cfg2b_state_get(io, "results");
        json_set(pa, "results", res ? json_copy(res) : json_null());
        json_t *r = cfg2b_seam(io, "_prompt_and_save_env", pa);
        if (cfg2b_raised(r)) { json_free(pairs); return r; }
        json_free(r);
        json_t *p3 = cfg2b_print(io, "", false);
        json_free(p3);
    }
    json_free(pairs);
    return json_null();
}

/* ====================================================================
 * 3. _offer_skill_config_vars  (config.py:1312)
 * =================================================================== */
/* PoP: cfg2b_offer_skill_config_vars @ hermes_cli/config.py:_offer_skill_config_vars */
static json_t *cfg2b_offer_skill_config_vars(cfg2b_flow_io_t *io, json_t *missing, json_t *constants) {
    size_t n = (missing && missing->type == JSON_ARRAY) ? json_len(missing) : 0;
    char cnt[32]; snprintf(cnt, sizeof cnt, "%zu", n);
    char *h1 = cfg2b_join("\n  ", cnt);
    char *heading = cfg2b_join(h1, " skill setting(s) not configured:"); free(h1);
    json_t *items = json_array();
    for (size_t i = 0; i < n; i++) {
        json_t *v = json_get(missing, i);
        json_t *skill = json_obj_get(v, "skill");
        const char *sk = (skill && cfg2b_truthy(skill)) ? cfg2b_sval(skill) : "unknown";
        char *l1 = cfg2b_join(cfg2b_sval(json_obj_get(v, "key")), CFG2B_EMDASH " ");
        char *l2 = cfg2b_join(l1, cfg2b_sval(json_obj_get(v, "description"))); free(l1);
        char *l3 = cfg2b_join(l2, " (from skill: "); free(l2);
        char *l4 = cfg2b_join(l3, sk); free(l3);
        char *l5 = cfg2b_join(l4, ")"); free(l4);
        json_append(items, json_string(l5)); free(l5);
    }
    json_t *oa = json_object();
    json_set(oa, "heading", json_string(heading)); free(heading);
    json_set(oa, "items", items);
    json_set(oa, "question", json_string("  Configure skill settings? [y/N]: "));
    json_t *accepted = cfg2b_seam(io, "_offer_list", oa);
    if (cfg2b_raised(accepted)) return accepted;
    bool yes = cfg2b_truthy(accepted); json_free(accepted);
    if (!yes) return json_null();
    json_t *cfg = cfg2b_seam(io, "read_raw_config", NULL);
    if (cfg2b_raised(cfg)) return cfg;
    bool impfail = json_get_bool(constants, "skill_config_prefix_import_raise", false);
    const char *prefix = impfail ? "skills.config"
        : json_get_str(constants, "skill_config_prefix", "skills.config");
    for (size_t i = 0; i < n; i++) {
        json_t *v = json_get(missing, i);
        json_t *defv = json_obj_get(v, "default");
        bool hasdef = defv && cfg2b_truthy(defv);
        char *dh1 = hasdef ? cfg2b_join(" (default: ", cfg2b_sval(defv)) : strdup("");
        char *dh2 = hasdef ? cfg2b_join(dh1, ")") : strdup(""); free(dh1);
        char *p1 = cfg2b_join("  ", cfg2b_sval(json_obj_get(v, "prompt")));
        char *p2 = cfg2b_join(p1, dh2); free(p1); free(dh2);
        char *p3 = cfg2b_join(p2, ": "); free(p2);
        json_t *la = json_object(); json_set(la, "prompt", json_string(p3)); free(p3);
        json_t *linput = cfg2b_seam(io, "line_input", la);
        if (cfg2b_raised(linput)) { json_free(cfg); return linput; }
        char *stripped = cfg2b_strip(cfg2b_sval(linput));
        json_free(linput);
        const char *defstr = defv ? cfg2b_sval(defv) : "";
        char *value = (stripped && stripped[0]) ? stripped : (hasdef ? strdup(defstr) : strdup(""));
        if (stripped && !stripped[0]) free(stripped);
        const char *key = cfg2b_sval(json_obj_get(v, "key"));
        if (value[0]) {
            char *dotted = cfg2b_join(prefix, ".");
            char *dotted2 = cfg2b_join(dotted, key); free(dotted);
            json_t *sa = json_object();
            json_set(sa, "config", json_copy(cfg));
            json_set(sa, "key", json_string(dotted2)); free(dotted2);
            json_set(sa, "value", json_string(value));
            json_t *sr = cfg2b_seam(io, "_set_nested", sa);
            if (cfg2b_raised(sr)) { free(value); json_free(cfg); return sr; }
            json_free(sr);
            json_t *live = cfg2b_results_list(io, "config_added");
            if (live && live->type == JSON_ARRAY) json_append(live, json_string(key));
            char *m1 = cfg2b_join("  " CFG2B_CHECK " Saved ", key);
            char *m2 = cfg2b_join(m1, " = "); free(m1);
            char *m3 = cfg2b_join(m2, value); free(m2);
            json_t *pr = cfg2b_print(io, m3, false); free(m3);
            if (cfg2b_raised(pr)) { free(value); json_free(cfg); return pr; }
            json_free(pr);
        } else {
            json_t *skill = json_obj_get(v, "skill");
            const char *sk = (skill && cfg2b_truthy(skill)) ? cfg2b_sval(skill) : "?";
            char *w1 = cfg2b_join("Skipped ", key);
            char *w2 = cfg2b_join(w1, " — skill '"); free(w1);
            char *w3 = cfg2b_join(w2, sk); free(w2);
            char *w4 = cfg2b_join(w3, "' may ask for it later"); free(w3);
            json_t *live = cfg2b_results_list(io, "warnings");
            if (live && live->type == JSON_ARRAY) json_append(live, json_string(w4));
            free(w4);
        }
        free(value);
        json_t *p4 = cfg2b_print(io, "", false); json_free(p4);
    }
    json_t *pa = json_object(); json_set(pa, "config", json_copy(cfg));
    json_free(cfg);
    json_t *r = cfg2b_seam(io, "_persist_migration", pa);
    return r;
}

/* ====================================================================
 * 4. _note_policy  (config.py:2325)
 * =================================================================== */
/* PoP: cfg2b_note_policy @ hermes_cli/config.py:_note_policy */
static json_t *cfg2b_note_policy(cfg2b_flow_io_t *io, const char *setter, json_t *raw, json_t *redact) {
    if (!redact || redact->type != JSON_OBJECT) return json_null();
    if (!json_get_bool(redact, "has_setter", false)) return json_null();
    json_t *h = cfg2b_seam(io, "hermes_home_key", NULL);
    if (cfg2b_raised(h)) return h;
    json_t *a = json_object();
    json_set(a, "setter", json_string(setter ? setter : ""));
    json_set(a, "raw", raw ? json_copy(raw) : json_null());
    json_set(a, "home", json_copy(h));
    json_free(h);
    json_t *r = cfg2b_seam(io, "note", a);
    return r;
}

/* ====================================================================
 * 5. _read_env_lines  (config.py:2384)
 * =================================================================== */
/* PoP: cfg2b_read_env_lines @ hermes_cli/config.py:_read_env_lines */
static json_t *cfg2b_read_env_lines(cfg2b_flow_io_t *io, const char *env_path) {
    json_t *a = json_object();
    json_set(a, "path", json_string(env_path ? env_path : ""));
    json_set(a, "encoding", json_string("utf-8-sig"));
    json_set(a, "errors", json_string("replace"));
    json_t *lines = cfg2b_seam(io, "open", a);
    if (cfg2b_raised(lines)) return lines;
    json_t *sa = json_object();
    json_set(sa, "lines", json_copy(lines));
    json_free(lines);
    json_t *r = cfg2b_seam(io, "sanitize_env_lines", sa);
    return r;
}

/* ====================================================================
 * 6. _env_write_lock_holder  (config.py:2395)
 * =================================================================== */
/* PoP: cfg2b_env_write_lock_holder @ hermes_cli/config.py:_env_write_lock_holder */
static json_t *cfg2b_env_write_lock_holder(cfg2b_flow_io_t *io, const char *env_path) {
    json_t *a = json_object();
    json_set(a, "key", json_string(env_path ? env_path : ""));
    json_t *r = cfg2b_seam(io, "lock_holder", a);
    return r;
}

/* ====================================================================
 * 7. _env_write_lock  (config.py:2404) — contextmanager generator
 * =================================================================== */
/* PoP: cfg2b_env_write_lock @ hermes_cli/config.py:_env_write_lock */
static json_t *cfg2b_env_write_lock(cfg2b_flow_io_t *io, const char *env_path) {
    /* holder is a real sibling (transcribed inline) — fires lock_holder. */
    json_t *holder = cfg2b_env_write_lock_holder(io, env_path);
    if (cfg2b_raised(holder)) return holder;
    /* lock path = env_path.with_name(name + ".lock") — FakePath.with_name in
     * the oracle is modeled by the fixture's "lock_path" field; build the
     * default here from the posix path. */
    const char *base = env_path ? env_path : "";
    char *name = cfg2b_join("", base);
    /* basename */
    char *slash = strrchr(name, '/');
    const char *bn = slash ? slash + 1 : name;
    char *lockname = cfg2b_join(bn, ".lock");
    char *lockpath = cfg2b_path_join(slash ? base : "", lockname);
    free(lockname); free(name);
    json_t *a = json_object();
    json_set(a, "path", json_string(lockpath));
    json_set(a, "holder", json_copy(holder));
    json_free(holder);
    json_set(a, "timeout", json_number(15.0));
    char *msg = cfg2b_join("Timed out waiting for the environment file lock (", base);
    char *msg2 = cfg2b_join(msg, ")"); free(msg);
    json_set(a, "message", json_string(msg2)); free(msg2);
    free(lockpath);
    json_t *r = cfg2b_seam(io, "file_lock", a);
    if (cfg2b_raised(r)) return r;
    json_free(r);
    json_t *out = json_array();
    json_append(out, json_null());   /* the single `yield` (None) */
    return out;
}

/* ====================================================================
 * 8. _write_env_lines  (config.py:2434)
 * =================================================================== */
/* PoP: cfg2b_write_env_lines @ hermes_cli/config.py:_write_env_lines */
static json_t *cfg2b_write_env_lines(cfg2b_flow_io_t *io, const char *env_path, json_t *lines, bool preserve_mode) {
    json_t *original_mode = NULL; /* int JSON number or NULL */
    if (preserve_mode) {
        json_t *a = json_object(); json_set(a, "path", json_string(env_path));
        json_t *st = cfg2b_seam(io, "path_stat", a);
        if (!cfg2b_raised(st)) {
            json_t *mode = json_obj_get(st, "mode");
            if (mode) original_mode = json_copy(mode);
            json_free(st);
        } else {
            json_free(st); /* OSError swallowed */
        }
    }
    json_t *ma = json_object();
    json_set(ma, "path", json_string(env_path));
    json_set(ma, "suffix", json_string(".tmp"));
    json_set(ma, "prefix", json_string(".env_"));
    json_t *mt = cfg2b_seam(io, "mkstemp_beside", ma);
    if (cfg2b_raised(mt)) return mt;
    double fd = cfg2b_val_num(json_get(mt, 0), 0);
    const char *tmp = cfg2b_sval(json_get(mt, 1));
    char *tmp_copy = strdup(tmp ? tmp : "");
    json_free(mt);
    json_t *last = NULL;
    json_t *fa = json_object();
    json_set(fa, "fd", json_number(fd));
    json_set(fa, "mode", json_string("w"));
    json_set(fa, "encoding", json_string("utf-8"));
    json_t *file = cfg2b_seam(io, "fdopen", fa);
    if (cfg2b_raised(file)) { last = file; goto cleanup; }
    json_t *wa = json_object();
    json_set(wa, "file", json_copy(file));
    json_set(wa, "lines", lines ? json_copy(lines) : json_array());
    json_t *wr = cfg2b_seam(io, "writelines", wa);
    if (cfg2b_raised(wr)) { last = wr; json_free(file); goto cleanup; }
    json_free(wr);
    json_t *fl = cfg2b_seam(io, "flush", NULL);
    if (cfg2b_raised(fl)) { last = fl; json_free(file); goto cleanup; }
    json_free(fl);
    json_t *fsa = json_object(); json_set(fsa, "fd", json_number(fd));
    json_t *fs = cfg2b_seam(io, "fsync", fsa);
    if (cfg2b_raised(fs)) { last = fs; json_free(file); goto cleanup; }
    json_free(fs);
    json_free(file);
    json_t *ara = json_object();
    json_set(ara, "tmp_path", json_string(tmp_copy));
    json_set(ara, "path", json_string(env_path));
    json_t *ar = cfg2b_seam(io, "atomic_replace", ara);
    if (cfg2b_raised(ar)) { last = ar; goto cleanup; }
    json_free(ar);
    /* success path */
    if (original_mode) {
        json_t *ca = json_object();
        json_set(ca, "path", json_string(env_path));
        json_set(ca, "mode", json_copy(original_mode));
        json_t *cr = cfg2b_seam(io, "chmod", ca);
        if (cfg2b_raised(cr)) json_free(cr); /* OSError swallowed */
        else json_free(cr);
        json_free(original_mode);
    } else {
        json_t *sa = json_object(); json_set(sa, "path", json_string(env_path));
        json_t *sr = cfg2b_seam(io, "secure_file", sa);
        if (cfg2b_raised(sr)) { json_free(sr); free(tmp_copy); return cfg2b_make_raise("OSError", "secure_file failed"); }
        json_free(sr);
    }
    free(tmp_copy);
    return json_null();

cleanup: {
    json_t *ua = json_object(); json_set(ua, "path", json_string(tmp_copy));
    json_t *ur = cfg2b_seam(io, "unlink", ua);
    if (cfg2b_raised(ur)) json_free(ur); /* OSError swallowed */
    else json_free(ur);
    free(tmp_copy);
    if (original_mode) json_free(original_mode);
    return last;
}
}

/* ====================================================================
 * 9. _cmd_config_migrate  (config.py:3451)
 * =================================================================== */
/* PoP: cfg2b_cmd_config_migrate @ hermes_cli/config.py:_cmd_config_migrate */
static json_t *cfg2b_cmd_config_migrate(cfg2b_flow_io_t *io) {
    json_t *b = cfg2b_seam(io, "print_banner", NULL);
    if (cfg2b_raised(b)) return b; 
    json_t *me = cfg2b_seam(io, "get_missing_env_vars", NULL);
    if (cfg2b_raised(me)) return me;
    json_t *mc = cfg2b_seam(io, "get_missing_config_fields", NULL);
    if (cfg2b_raised(mc)) { json_free(me); return mc; }
    json_t *cv = cfg2b_seam(io, "check_config_version", NULL);
    if (cfg2b_raised(cv)) { json_free(me); json_free(mc); return cv; }
    double cur = cfg2b_val_num(json_get(cv, 0), 0), latest = cfg2b_val_num(json_get(cv, 1), 0);
    json_free(cv);
    size_t nenv = (me && me->type == JSON_ARRAY) ? json_len(me) : 0;
    size_t nmc = (mc && mc->type == JSON_ARRAY) ? json_len(mc) : 0;
    if (nenv == 0 && nmc == 0 && cur >= latest) {
        cfg2b_cp(io, "\xe2\x9c\x93 Configuration is up to date!", CFG2B_GREEN);
        json_t *q = cfg2b_print(io, "", false); json_free(q);
        json_free(me); json_free(mc);
        return json_null();
    }
    if (cur < latest) {
        char c1[32], c2[32]; snprintf(c1, sizeof c1, "%.0f", cur); snprintf(c2, sizeof c2, "%.0f", latest);
        char *m1 = cfg2b_join("  Config version: ", c1);
        char *m2 = cfg2b_join(m1, " \xe2\x86\x92 "); free(m1);
        char *m3 = cfg2b_join(m2, c2); free(m2);
        json_t *p = cfg2b_print(io, m3, false); free(m3);
        if (cfg2b_raised(p)) { json_free(me); json_free(mc); return p; }
        json_free(p);
    }
    if (nmc) {
        char c1[32]; snprintf(c1, sizeof c1, "%zu", nmc);
        char *m1 = cfg2b_join("\n  ", c1);
        char *m2 = cfg2b_join(m1, " new config option(s) will be added with defaults"); free(m1);
        json_t *p = cfg2b_print(io, m2, false); free(m2);
        if (cfg2b_raised(p)) { json_free(me); json_free(mc); return p; }
        json_free(p);
    }
    /* required / optional groups */
    json_t *req = json_array(), *opt = json_array();
    for (size_t i = 0; i < nenv; i++) {
        json_t *v = json_get(me, i);
        bool is_req = json_get_bool(v, "is_required", false);
        bool adv = json_get_bool(v, "advanced", false);
        if (is_req) json_append(req, json_copy(v));
        else if (!adv) json_append(opt, json_copy(v));
    }
    /* group 1: required */
    char c1[32];
    if (json_len(req)) {
    snprintf(c1, sizeof c1, "%zu", json_len(req));
    char *h1 = cfg2b_join("\n  \xe2\x9a\xa0\xef\xb8\x8f  ", c1);
    char *h2 = cfg2b_join(h1, " required API key(s) missing:"); free(h1);
    cfg2b_print(io, h2, false); free(h2);
    }
    for (size_t i = 0; i < json_len(req); i++) {
        json_t *v = json_get(req, i);
        const char *nm = cfg2b_sval(json_obj_get(v, "name"));
        char *l1 = cfg2b_join("     \xe2\x80\xa2 ", nm);
        json_t *p = cfg2b_print(io, l1, false); free(l1);
        if (cfg2b_raised(p)) { json_free(req); json_free(opt); json_free(me); json_free(mc); return p; }
        json_free(p);
    }
    /* group 2: optional (uses _tools_suffix via suffix " (enables: {})") */
    if (json_len(opt)) {
    snprintf(c1, sizeof c1, "%zu", json_len(opt));
    char *g1 = cfg2b_join("\n  \xe2\x84\xb9\xef\xb8\x8f  ", c1);
    char *g2 = cfg2b_join(g1, " optional API key(s) not configured:"); free(g1);
    cfg2b_print(io, g2, false); free(g2);
    }
    for (size_t i = 0; i < json_len(opt); i++) {
        json_t *v = json_get(opt, i);
        const char *nm = cfg2b_sval(json_obj_get(v, "name"));
        json_t *sa = json_object(); json_set(sa, "info", json_copy(v));
        json_set(sa, "fmt", json_string(" (enables: {})"));
        json_t *sf = cfg2b_seam(io, "tools_suffix", sa);
        if (cfg2b_raised(sf)) { json_free(req); json_free(opt); json_free(me); json_free(mc); return sf; }
        char *l1 = cfg2b_join("     \xe2\x80\xa2 ", nm);
        char *l2 = cfg2b_join(l1, cfg2b_sval(sf)); free(l1); json_free(sf);
        json_t *p = cfg2b_print(io, l2, false); free(l2);
        if (cfg2b_raised(p)) { json_free(req); json_free(opt); json_free(me); json_free(mc); return p; }
        json_free(p);
    }
    json_free(req); json_free(opt); json_free(me); json_free(mc);
    json_t *pb = cfg2b_print(io, "", false); json_free(pb);
    json_t *mr = cfg2b_seam(io, "migrate_config", NULL);
    if (cfg2b_raised(mr)) return mr;
    json_t *pa = cfg2b_print(io, "", false); json_free(pa);
    bool stopped = json_obj_get(mr, "failed_step") != NULL && !cfg2b_is_null(json_obj_get(mr, "failed_step"));
    bool added = cfg2b_truthy(json_obj_get(mr, "env_added")) || cfg2b_truthy(json_obj_get(mr, "config_added"));
    if (added && !stopped) {
        cfg2b_cp(io, "\xe2\x9c\x93 Configuration updated!", CFG2B_GREEN);
    }
    json_t *warn = json_obj_get(mr, "warnings");
    size_t nw = (warn && warn->type == JSON_ARRAY) ? json_len(warn) : 0;
    if (nw) {
        json_t *p = cfg2b_print(io, "", false); json_free(p);
        for (size_t i = 0; i < nw; i++) {
            char *w1 = cfg2b_join("  \xe2\x9a\xa0\xef\xb8\x8f  ", cfg2b_sval(json_get(warn, i)));
            cfg2b_cp(io, w1, CFG2B_YELLOW); free(w1);
        }
    }
    json_free(mr);
    json_t *pf = cfg2b_print(io, "", false); json_free(pf);
    if (stopped) return cfg2b_make_raise_code("SystemExit", 1);
    return json_null();
}

/* ====================================================================
 * 10. _cmd_config_check  (config.py:3494)
 * =================================================================== */
/* PoP: cfg2b_cmd_config_check @ hermes_cli/config.py:_cmd_config_check */
static json_t *cfg2b_cmd_config_check(cfg2b_flow_io_t *io, json_t *constants) {
    json_t *b = cfg2b_seam(io, "print_banner", NULL);
    if (cfg2b_raised(b)) return b; 
    json_t *cv = cfg2b_seam(io, "check_config_version", NULL);
    if (cfg2b_raised(cv)) return cv;
    double cur = cfg2b_val_num(json_get(cv, 0), 0), latest = cfg2b_val_num(json_get(cv, 1), 0);
    json_free(cv);
    char c1[32], c2[32]; snprintf(c1, sizeof c1, "%.0f", cur); snprintf(c2, sizeof c2, "%.0f", latest);
    if (cur >= latest) {
        char *m = cfg2b_join("  Config version: ", c1);
        char *m2 = cfg2b_join(m, " \xe2\x9c\x93"); free(m);
        json_t *p = cfg2b_print(io, m2, false); free(m2);
        if (cfg2b_raised(p)) return p; 
    } else {
        char *m = cfg2b_join("  Config version: ", c1);
        char *m2 = cfg2b_join(m, " \xe2\x86\x92 "); free(m);
        char *m3 = cfg2b_join(m2, c2); free(m2);
        char *m4 = cfg2b_join(m3, " (update available)"); free(m3);
        cfg2b_cp(io, m4, CFG2B_YELLOW); free(m4);
    }
    json_t *required = json_obj_get(constants, "required_env_vars");
    json_t *optional = json_obj_get(constants, "optional_env_vars");
    /* two groups */
    for (int g = 0; g < 2; g++) {
        json_t *table = g == 0 ? required : optional;
        const char *title = g == 0 ? "Required:" : "Optional:";
        const char *code = g == 0 ? CFG2B_BOLD : CFG2B_BOLD;
        json_t *p0 = cfg2b_print(io, "", false); if (cfg2b_raised(p0)) return p0; 
        char *t1 = cfg2b_join("  ", title);
        cfg2b_cp(io, t1, code); free(t1);
        size_t n = (table && table->type == JSON_OBJECT) ? table->c.count : 0;
        for (size_t i = 0; i < n; i++) {
            const char *nm = table->c.keys[i];
            json_t *info = table->c.items[i];
            json_t *a = json_object(); json_set(a, "name", json_string(nm));
            json_t *ev = cfg2b_seam(io, "get_env_value", a);
            if (cfg2b_raised(ev)) return ev;
            bool present = cfg2b_truthy(ev); json_free(ev);
            if (present) {
                char *l = cfg2b_join("    \xe2\x9c\x93 ", nm);
                json_t *p = cfg2b_print(io, l, false); free(l);
                if (cfg2b_raised(p)) return p; 
            } else if (g == 0) {
                char *l = cfg2b_join("    \xe2\x9c\x97 ", nm);
                char *l2 = cfg2b_join(l, " (missing)"); free(l);
                cfg2b_cp(io, l2, CFG2B_RED); free(l2);
            } else {
                json_t *sa = json_object(); json_set(sa, "info", json_copy(info));
                json_set(sa, "fmt", json_string(" \xe2\x86\x92 {}"));
                json_t *sf = cfg2b_seam(io, "tools_suffix", sa);
                if (cfg2b_raised(sf)) return sf;
                char *l = cfg2b_join("    \xe2\x97\x8b ", nm);
                char *l2 = cfg2b_join(l, cfg2b_sval(sf)); free(l); json_free(sf);
                cfg2b_cp(io, l2, CFG2B_DIM); free(l2);
            }
        }
    }
    json_t *mc = cfg2b_seam(io, "get_missing_config_fields", NULL);
    if (cfg2b_raised(mc)) return mc;
    size_t nmc = (mc && mc->type == JSON_ARRAY) ? json_len(mc) : 0;
    if (nmc) {
        char c[32]; snprintf(c, sizeof c, "%zu", nmc);
        json_t *p = cfg2b_print(io, "", false); if (cfg2b_raised(p)) { json_free(mc); return p; } 
        char *m1 = cfg2b_join("  ", c);
        char *m2 = cfg2b_join(m1, " new config option(s) available"); free(m1);
        cfg2b_cp(io, m2, CFG2B_YELLOW); free(m2);
        json_t *pd = cfg2b_print(io, "    Run 'hermes config migrate' to add them", false);
        if (cfg2b_raised(pd)) { json_free(mc); return pd; } 
    }
    json_free(mc);
    json_t *rr = cfg2b_seam(io, "read_raw_config_readonly", NULL);
    if (cfg2b_raised(rr)) return rr;
    json_t *da = json_object();
    json_set(da, "config", json_copy(rr)); json_free(rr);
    json_t *diag = cfg2b_seam(io, "config_check_diagnostics", da);
    if (cfg2b_raised(diag)) return diag;
    size_t nd = (diag && diag->type == JSON_ARRAY) ? json_len(diag) : 0;
    if (nd) {
        json_t *p = cfg2b_print(io, "", false); if (cfg2b_raised(p)) { json_free(diag); return p; } 
        cfg2b_cp(io, "  Saved configuration:", CFG2B_BOLD);
        for (size_t i = 0; i < nd; i++) {
            char *l = cfg2b_join("    \xe2\x9a\xa0 ", cfg2b_sval(json_get(diag, i)));
            cfg2b_cp(io, l, CFG2B_YELLOW); free(l);
        }
    }
    json_free(diag);
    json_t *pf = cfg2b_print(io, "", false); json_free(pf);
    return json_null();
}

/* ====================================================================
 * 11. _platform_manifest_paths  (config.py:3589) — generator
 * =================================================================== */
/* PoP: cfg2b_platform_manifest_paths @ hermes_cli/config.py:_platform_manifest_paths */
static json_t *cfg2b_platform_manifest_paths(cfg2b_flow_io_t *io, const char *home, const char *source) {
    json_t *out = json_array();
    /* roots: [(get_project_root()/plugins/platforms, False), ...] */
    json_t *roots = json_array();
    bool all = !source || strcmp(source, "all") == 0;
    bool bundled = all || (source && strcmp(source, "bundled") == 0);
    bool user = all || (source && strcmp(source, "user") == 0);
    if (bundled) {
        json_t *pr = cfg2b_seam(io, "get_project_root", NULL);
        if (cfg2b_raised(pr)) { json_free(roots); return pr; }
        char *p1 = cfg2b_path_join(cfg2b_sval(pr), "plugins");
        char *p2 = cfg2b_path_join(p1, "platforms"); free(p1); json_free(pr);
        json_t *r = json_array(); json_append(r, json_string(p2)); json_append(r, json_bool(false)); free(p2);
        json_append(roots, r);
    }
    if (user) {
        const char *h = (home && home[0]) ? home : NULL;
        json_t *hh = NULL;
        if (!h) { hh = cfg2b_seam(io, "get_hermes_home", NULL); if (cfg2b_raised(hh)) { json_free(roots); return hh; } h = cfg2b_sval(hh); }
        char *up = cfg2b_path_join(h, "plugins");
        char *upp = cfg2b_path_join(up, "platforms"); free(up);
        json_t *r1 = json_array(); json_append(r1, json_string(upp)); json_append(r1, json_bool(false)); free(upp);
        json_append(roots, r1);
        json_t *r2 = json_array(); json_append(r2, json_string(up ? "" : "")); /* placeholder replaced below */
        /* user_plugins root itself with require_kind True */
        char *up2 = cfg2b_path_join(h, "plugins");
        json_t *r3 = json_array(); json_append(r3, json_string(up2)); json_append(r3, json_bool(true)); free(up2);
        json_append(roots, r3);
        json_free(r2);
        if (hh) json_free(hh);
    }
    for (size_t ri = 0; ri < json_len(roots); ri++) {
        json_t *rootpair = json_get(roots, ri);
        const char *root = cfg2b_sval(json_get(rootpair, 0));
        bool require_kind = cfg2b_val_bool(json_get(rootpair, 1), false);
        json_t *sa = json_object(); json_set(sa, "path", json_string(root));
        json_t *scan = cfg2b_seam(io, "os.scandir", sa);
        if (cfg2b_raised(scan)) {
            json_t *spec = cfg2b_raise_spec(scan);
            const char *ty = spec ? json_get_str(spec, "type", "") : "";
            bool skip = strcmp(ty, "FileNotFoundError") == 0 || strcmp(ty, "NotADirectoryError") == 0;
            if (!skip) {
                json_t *t = json_array();
                json_append(t, json_string(root)); json_append(t, json_null());
                json_append(t, json_bool(require_kind)); json_append(t, json_copy(spec));
                json_append(out, t);
            }
            json_free(scan);
            continue;
        }
        size_t ne = (scan && scan->type == JSON_ARRAY) ? json_len(scan) : 0;
        for (size_t ei = 0; ei < ne; ei++) {
            json_t *e = json_get(scan, ei);
            const char *ename = cfg2b_sval(json_obj_get(e, "name"));
            /* _is_plugin_dir_name (pure, transcribed inline) */
            bool ok = !(strcmp(ename, "__pycache__") == 0 || strcmp(ename, "__init__") == 0 ||
                        strcmp(ename, "__") == 0);
            if (!ok) continue;
            bool isdir = cfg2b_val_bool(json_obj_get(e, "is_dir"), false);
            if (json_obj_get(e, "is_dir_raise")) { bool ir = cfg2b_val_bool(json_obj_get(e, "is_dir"), false);
                /* OSError -> continue */ json_free(scan); (void)ir; continue; }
            if (!isdir) continue;
            const char *epath = cfg2b_sval(json_obj_get(e, "path"));
            const char *files[2] = {"plugin.yaml", "plugin.yml"};
            for (int fi = 0; fi < 2; fi++) {
                char *p = cfg2b_path_join(epath, files[fi]);
                json_t *sta = json_object(); json_set(sta, "path", json_string(p));
                json_t *st = cfg2b_seam(io, "path_stat", sta);
                if (cfg2b_raised(st)) {
                    json_t *spec = cfg2b_raise_spec(st);
                    const char *ty = spec ? json_get_str(spec, "type", "") : "";
                    if (strcmp(ty, "PermissionError") == 0) {
                        json_t *t = json_array();
                        json_append(t, json_string(ename)); json_append(t, json_null());
                        json_append(t, json_bool(require_kind)); json_append(t, json_copy(spec));
                        json_append(out, t);
                        json_free(st); free(p); break;
                    }
                    json_free(st); free(p); continue; /* other OSError -> next file */
                }
                double mode = cfg2b_val_num(json_obj_get(st, "mode"), 0);
                bool isreg = ((long)mode & 0170000) == 0100000; /* S_ISREG */
                json_free(st);
                if (!isreg) { free(p); continue; }
                json_t *t = json_array();
                json_append(t, json_string(ename));
                json_append(t, json_string(p));
                json_append(t, json_bool(require_kind));
                json_t *sd = json_object(); json_set(sd, "st_mode", json_number(mode));
                json_t *stp = json_object(); json_set(stp, "__stat__", sd);
                json_append(t, stp);
                json_append(out, t);
                free(p);
                break;
            }
        }
        json_free(scan);
    }
    json_free(roots);
    return out;
}

/* ====================================================================
 * 12. platform_manifest_stamp  (config.py:3633)
 * =================================================================== */
/* PoP: cfg2b_manifest_stamp @ hermes_cli/config.py:platform_manifest_stamp */
static json_t *cfg2b_manifest_stamp(cfg2b_flow_io_t *io, const char *home) {
    json_t *paths = cfg2b_platform_manifest_paths(io, home, "user");
    json_t *out = json_array();
    size_t n = (paths && paths->type == JSON_ARRAY) ? json_len(paths) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *t = json_get(paths, i);
        const char *name = cfg2b_sval(json_get(t, 0));
        json_t *path = json_get(t, 1);
        json_t *st = json_get(t, 3);
        json_t *o = json_array();
        json_append(o, json_string(name));
        if (!cfg2b_is_null(path)) {
            json_append(o, json_copy(path));
            json_t *sa = json_object(); json_set(sa, "st", json_copy(st));
            json_t *sig = cfg2b_seam(io, "file_signature", sa);
            if (cfg2b_raised(sig)) { json_free(o); json_free(paths); return sig; }
            json_append(o, json_copy(sig)); json_free(sig);
        } else {
            json_append(o, json_string("None"));
            json_t *stp = st ? json_obj_get(st, "__stat__") : NULL;
            const char *excty = st ? json_get_str(st, "type", NULL) : NULL;
            json_append(o, json_string(stp ? "stat_result" : (excty ? excty : "stat_result")));
        }
        json_append(out, o);
    }
    json_free(paths);
    return out;
}

/* ====================================================================
 * 13. _platform_plugin_manifests  (config.py:3641) — generator
 * =================================================================== */
/* PoP: cfg2b_platform_plugin_manifests @ hermes_cli/config.py:_platform_plugin_manifests */
static json_t *cfg2b_platform_plugin_manifests(cfg2b_flow_io_t *io, const char *home, const char *source,
                                        bool strict, json_t *skipped) {
    json_t *sw = cfg2b_seam(io, "safe_worker_enabled", NULL);
    if (cfg2b_raised(sw)) return sw;
    bool safe = cfg2b_truthy(sw); json_free(sw);
    if (safe) return json_array();
    json_t *paths = cfg2b_platform_manifest_paths(io, home, source);
    json_t *out = json_array();
    size_t n = (paths && paths->type == JSON_ARRAY) ? json_len(paths) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *t = json_get(paths, i);
        const char *dn = cfg2b_sval(json_get(t, 0));
        json_t *mp = json_get(t, 1);
        bool require_kind = cfg2b_val_bool(json_get(t, 2), false);
        json_t *st = json_get(t, 3);
        if (cfg2b_is_null(mp)) {
            json_t *a = json_object();
            json_set(a, "fmt", json_string("Skipping unreadable plugin directory %s: %s"));
            json_t *args = json_array(); json_append(args, json_string(dn));
            json_append(args, json_string(json_get_str(json_obj_get(st, "__raise__"), "message", ""))); json_set(a, "args", args);
            json_t *w = cfg2b_seam(io, "logger.warning", a);
            if (cfg2b_raised(w)) { json_free(paths); return w; }
            json_free(w);
            if (skipped && skipped->type == JSON_ARRAY) json_append(skipped, json_string(dn));
            continue;
        }
        json_t *oa = json_object();
        json_set(oa, "path", json_copy(mp));
        json_set(oa, "encoding", json_string("utf-8-sig"));
        json_t *fh = cfg2b_seam(io, "open", oa);
        if (cfg2b_raised(fh)) {
            bool is_os = false;
            json_t *spec = cfg2b_raise_spec(fh);
            const char *ty = spec ? json_get_str(spec, "type", "") : "";
            is_os = cfg2b_exc_is_exception(ty) && (
                strcmp(ty, "OSError") == 0 || strcmp(ty, "FileNotFoundError") == 0 ||
                strcmp(ty, "PermissionError") == 0 || strcmp(ty, "IsADirectoryError") == 0 ||
                strcmp(ty, "NotADirectoryError") == 0);
            if (is_os) {
                if (strict && !require_kind) { json_free(paths); return fh; }
                json_t *a = json_object();
                json_set(a, "fmt", json_string("Skipping unreadable plugin manifest %s: %s"));
                json_t *args = json_array(); json_append(args, json_copy(mp));
                json_append(args, json_string(spec ? json_get_str(spec, "message", "") : ""));
                json_set(a, "args", args);
                json_t *w = cfg2b_seam(io, "logger.warning", a); json_free(w);
                if (skipped && skipped->type == JSON_ARRAY) json_append(skipped, json_copy(mp));
                json_free(fh); continue;
            }
            /* generic Exception -> skipped only */
            if (skipped && skipped->type == JSON_ARRAY) json_append(skipped, json_copy(mp));
            json_free(fh); continue;
        }
        json_free(fh);
        json_t *la = json_object(); json_set(la, "file", json_copy(mp));
        json_t *man = cfg2b_seam(io, "fast_safe_load", la);
        if (cfg2b_raised(man)) {
            json_t *spec = cfg2b_raise_spec(man);
            const char *ty = spec ? json_get_str(spec, "type", "") : "";
            bool is_os = strcmp(ty, "OSError") == 0 || strcmp(ty, "FileNotFoundError") == 0 ||
                         strcmp(ty, "PermissionError") == 0 || strcmp(ty, "IsADirectoryError") == 0;
            if (is_os) {
                if (strict && !require_kind) { json_free(paths); return man; }
                json_t *a = json_object();
                json_set(a, "fmt", json_string("Skipping unreadable plugin manifest %s: %s"));
                json_t *args = json_array(); json_append(args, json_copy(mp));
                json_append(args, json_string(json_get_str(spec, "message", "")));
                json_set(a, "args", args);
                json_t *w = cfg2b_seam(io, "logger.warning", a); json_free(w);
                if (skipped && skipped->type == JSON_ARRAY) json_append(skipped, json_copy(mp));
                json_free(man); continue;
            }
            if (skipped && skipped->type == JSON_ARRAY) json_append(skipped, json_copy(mp));
            json_free(man); continue;
        }
        if (cfg2b_is_null(man)) { json_free(man); man = json_object(); }
        bool isdict = man->type == JSON_OBJECT;
        json_t *kind = isdict ? json_obj_get(man, "kind") : NULL;
        bool kindok = !require_kind || (kind && strcmp(cfg2b_sval(kind), "platform") == 0);
        if (!isdict || !kindok) { json_free(man); continue; }
        json_t *pair = json_array();
        json_append(pair, json_string(dn));
        json_append(pair, json_copy(man));
        json_append(out, pair);
        json_free(man);
    }
    json_free(paths);
    return out;
}
