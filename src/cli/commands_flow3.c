/*
 * commands_flow3.c — the remainder of the hermes_cli/commands.py FLOW surface
 * beyond the registry cluster already ported in commands_flow.c (flow1).
 *
 * Ports the 10 live defs NOT carried by flow1's FLOW core:
 *   infer_argument_mode, command_desktop_meta, resolve_command,
 *   _build_description, is_gateway_known_command,
 *   is_interrupt_then_dispatch, should_bypass_active_session,
 *   _resolve_config_gates, gateway_help_lines, _iter_plugin_command_entries.
 * (flow1 owns _localized / category_label / CommandDef.describe /
 * desktop_surface_registry / build_commands[_by_category] /
 * _is_gateway_available / __getattr__; those are SEAMED here, never re-ported.)
 *
 * Self-contained C11, libjson-only, harness-#included. Every live effect enters
 * through ONE universal seam:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `args` is borrowed (the seam copies what it keeps); the result is a NEW
 * json_t* (NULL == None; {"__raise__": cls, "msg": ...} == a raised exception).
 * The harness records one {"ev","args","ret"} event per call; the oracle's
 * recording stubs emit the identical shape while driving LIVE Python.
 *
 * Exception propagation: an UNCAUGHT seam raise parks its spec in the static
 * cm3_pending channel and the core returns a sentinel; the harness emits the
 * parked spec as ret. `except Exception` sites (_resolve_config_gates and
 * _iter_plugin_command_entries import/config guards) walk the FULL builtin
 * exception hierarchy below: every Exception subclass is swallowed, but the
 * BaseException trio (SystemExit / KeyboardInterrupt / GeneratorExit) passes
 * through. Fail-closed: a missing/empty seam script yields a RuntimeError spec.
 *
 * Seam name table:
 *   describe                  cmd.describe() -> str
 *   t                         agent.i18n.t(key, **kw) -> str
 *   _resolve_config_gates     commands._resolve_config_gates() -> [name]
 *   _is_gateway_available     commands._is_gateway_available(cmd, overrides) -> bool
 *   _iter_plugin_command_entries  commands._iter_plugin_command_entries() -> [[name,desc,hint]]
 *   read_raw_config           hermes_cli.config.read_raw_config() -> cfg
 *   cfg_get                   hermes_cli.config.cfg_get(cfg, *parts) -> value
 *   is_truthy_value           hermes_cli.config.is_truthy_value(value, default=False) -> bool
 *   get_plugin_commands       hermes_cli.plugins.get_plugin_commands() -> {name: meta}
 *
 * Import gates: fixture "import_gates" {module: true} models sys.modules[mod]=None
 *   -> ModuleNotFoundError "import of <mod> halted; None in sys.modules",
 *   raised at the from-import (inside try for both config and plugins guards).
 *
 * Modeling domain: registry/CommandDef field dicts are INJECTED per case; the
 * live 90-entry COMMAND_REGISTRY is outside the fixture domain. String helpers
 * are ASCII (lower/strip/replace); UTF-8 bytes pass through untouched.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include "json.h"

/* ── universal seam plumbing ── */
typedef struct cm3_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} cm3_seams_t;

static json_t *cm3_pending = NULL;

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
static bool jpresent(const json_t *v) { return v && v->type != JSON_NULL; }
static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool is_str(const json_t *v) { return v && v->type == JSON_STRING; }

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

static json_t *cm3_call(const cm3_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}
static const char *cm3_raise_class(const json_t *r) {
    if (is_obj(r)) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (is_str(c)) return c->str_val;
    }
    return NULL;
}
#define CM3_RAISED(r) (cm3_raise_class(r) != NULL)
static void cm3_park(json_t *r) { cm3_pending = r; }
static json_t *cm3_take_pending(void) { json_t *r = cm3_pending; cm3_pending = NULL; return r; }
static json_t *cm3_make_raise(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static void cm3_park_new(const char *cls, const char *msg) {
    cm3_park(cm3_make_raise(cls, msg));
}

/* ── FULL builtin exception hierarchy ──
 * `except Exception` walks parents; the BaseException trio never matches
 * Exception. Mirrors the oracle's BUILTIN_EXC / BASE_EXC tables. */
static const char *cm3_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        {"BaseException", NULL},
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"},
        {"Exception", "BaseException"},
        {"RuntimeError", "Exception"}, {"NotImplementedError", "RuntimeError"},
        {"RecursionError", "RuntimeError"},
        {"ValueError", "Exception"}, {"TypeError", "Exception"},
        {"AttributeError", "Exception"}, {"NameError", "Exception"},
        {"UnboundLocalError", "NameError"},
        {"LookupError", "Exception"}, {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
        {"ArithmeticError", "Exception"}, {"ZeroDivisionError", "ArithmeticError"},
        {"OverflowError", "ArithmeticError"}, {"FloatingPointError", "ArithmeticError"},
        {"OSError", "Exception"},
        {"FileNotFoundError", "OSError"}, {"PermissionError", "OSError"},
        {"IsADirectoryError", "OSError"}, {"NotADirectoryError", "OSError"},
        {"FileExistsError", "OSError"}, {"InterruptedError", "OSError"},
        {"BlockingIOError", "OSError"}, {"ChildProcessError", "OSError"},
        {"ProcessLookupError", "OSError"}, {"TimeoutError", "OSError"},
        {"ConnectionError", "OSError"}, {"BrokenPipeError", "ConnectionError"},
        {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
        {"ConnectionBrokenError", "OSError"},
        {"JSONDecodeError", "ValueError"}, {"UnicodeError", "ValueError"},
        {"UnicodeDecodeError", "UnicodeError"}, {"UnicodeEncodeError", "UnicodeError"},
        {"UnicodeTranslateError", "UnicodeError"},
        {"StopIteration", "Exception"}, {"StopAsyncIteration", "Exception"},
        {"AssertionError", "Exception"}, {"SystemError", "Exception"},
        {"ReferenceError", "Exception"}, {"MemoryError", "Exception"},
        {"BufferError", "Exception"}, {"EOFError", "Exception"},
        {"ImportError", "Exception"}, {"ModuleNotFoundError", "ImportError"},
        {"SyntaxError", "Exception"}, {"IndentationError", "SyntaxError"},
        {"TabError", "IndentationError"},
        {"Warning", "Exception"}, {"UserWarning", "Warning"},
        {"DeprecationWarning", "Warning"}, {"PendingDeprecationWarning", "Warning"},
        {"SyntaxWarning", "Warning"}, {"RuntimeWarning", "Warning"},
        {"FutureWarning", "Warning"}, {"ImportWarning", "Warning"},
        {"UnicodeWarning", "Warning"}, {"BytesWarning", "Warning"},
        {"ResourceWarning", "Warning"}, {"EncodingWarning", "Warning"},
    };
    if (!c) return NULL;
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (!strcmp(T[i].c, c)) return T[i].p;
    return NULL;
}
static bool cm3_exc_matches(const json_t *spec, const char *const *classes, size_t n) {
    const char *cls = cm3_raise_class(spec);
    if (!cls) return false;
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < n; i++)
            if (!strcmp(cls, classes[i])) return true;
        const char *p = cm3_exc_parent(cls);
        if (!p) return false;
        cls = p;
    }
    return false;
}
/* except Exception (NOT the BaseException trio). */
static bool cm3_exc_is_exception(const json_t *spec) {
    static const char *E[] = {"Exception"};
    return cm3_exc_matches(spec, E, 1);
}

/* ── args builders ── */
static json_t *a0(void) { return json_object(); }
static void aset_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void aset_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void aset_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}

/* ── ASCII string helpers ── */
static char *cm3_lower(const char *s) {
    if (!s) return strdup("");
    size_t n = strlen(s);
    char *out = malloc(n + 1);
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        out[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    }
    out[n] = '\0';
    return out;
}
/* name.lower().lstrip("/") */
static char *cm3_query_key(const char *s) {
    char *low = cm3_lower(s);
    const char *p = low;
    while (*p == '/') p++;
    char *out = strdup(p);
    free(low);
    return out;
}
static bool cm3_is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
/* Python str.strip() (ASCII whitespace domain). */
static char *cm3_strip(const char *s) {
    if (!s) return strdup("");
    const char *b = s, *e = s + strlen(s);
    while (b < e && cm3_is_ws(*b)) b++;
    while (e > b && cm3_is_ws(*(e - 1))) e--;
    size_t n = (size_t)(e - b);
    char *out = malloc(n + 1);
    memcpy(out, b, n);
    out[n] = '\0';
    return out;
}
/* str(v) for domain values: str / integral int / bool / None. */
static char *cm3_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) return xsf("%.0f", v->num_val);
    return strdup("");  /* containers: outside the fixture domain */
}
/* replace '-' with '_' (alias internal-name comparison). */
static char *cm3_dash2us(const char *s) {
    if (!s) return strdup("");
    char *out = strdup(s);
    for (char *p = out; *p; p++) if (*p == '-') *p = '_';
    return out;
}
/* split a dotted config path into a JSON array of string segments. */
static json_t *cm3_split_dots(const char *s) {
    json_t *a = json_array();
    if (!s) return a;
    const char *p = s;
    while (*p) {
        const char *b = p;
        while (*p && *p != '.') p++;
        size_t n = (size_t)(p - b);
        char *seg = malloc(n + 1);
        memcpy(seg, b, n);
        seg[n] = '\0';
        json_append(a, json_string(seg));
        free(seg);
        if (*p == '.') p++;
    }
    return a;
}
static bool cm3_arr_has_str(const json_t *a, const char *s) {
    if (!is_arr(a) || !s) return false;
    for (size_t i = 0; i < json_len(a); i++) {
        const char *x = jstr(json_get(a, i));
        if (x && !strcmp(x, s)) return true;
    }
    return false;
}
static int cm3_str_cmp(const void *pa, const void *pb) {
    const json_t *a = *(const json_t *const *)pa;
    const json_t *b = *(const json_t *const *)pb;
    return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "");
}
static void cm3_sort_strs(json_t *arr) {
    if (is_arr(arr) && arr->c.count > 1)
        qsort(arr->c.items, arr->c.count, sizeof(json_t *), cm3_str_cmp);
}

/* ── import gates (sys.modules[mod] = None) ── */
static bool cm3_import_gated(const json_t *inp, const char *mod) {
    const json_t *gates = json_obj_get(inp, "import_gates");
    return jtruth(json_obj_get(gates, mod));
}
static void cm3_park_import_halt(const char *mod) {
    char *m = xsf("import of %s halted; None in sys.modules", mod);
    cm3_park_new("ModuleNotFoundError", m);
    free(m);
}

/* ══ hermes_cli/commands.py (remainder) ══ */

/* PoP: cm3_infer_argument_mode @ hermes_cli/commands.py:infer_argument_mode */
static json_t *cm3_infer_argument_mode(const json_t *cmd) {
    const json_t *am = json_obj_get(cmd, "argument_mode");
    const char *a = jstr(am);
    if (a && (!strcmp(a, "options") || !strcmp(a, "text") || !strcmp(a, "mixed")))
        return json_copy(am);
    const json_t *ah = json_obj_get(cmd, "args_hint");
    const char *hs = jstr(ah);
    char *hint = cm3_strip(hs ? hs : "");
    json_t *ret = NULL;
    if (jtruth(json_obj_get(cmd, "subcommands"))) {
        char *low = cm3_lower(hint);
        static const char *HINTS[] = {"<prompt>", "[text", "instructions", "[interval]", "<what"};
        bool prose = false;
        if (hint[0]) {
            for (size_t i = 0; i < 5; i++)
                if (strstr(low, HINTS[i])) { prose = true; break; }
        }
        ret = json_string(prose ? "mixed" : "options");
        free(low);
    } else if (hint[0]) {
        ret = json_string("text");
    }
    free(hint);
    return ret;  /* NULL == None */
}

/* PoP: cm3_command_desktop_meta @ hermes_cli/commands.py:command_desktop_meta */
static json_t *cm3_command_desktop_meta(const json_t *cmd) {
    json_t *meta = json_object();
    json_t *am = cm3_infer_argument_mode(cmd);
    json_set(meta, "argument_mode", am ? am : json_null());
    const json_t *d = json_obj_get(cmd, "desktop");
    json_set(meta, "desktop", d ? json_copy(d) : json_null());
    const json_t *ds = json_obj_get(cmd, "desktop_subcommands");
    if (ds && ds->type != JSON_NULL)
        json_set(meta, "desktop_subcommands", is_arr(ds) ? json_copy(ds) : json_null());
    return meta;
}

/* PoP: cm3_resolve_command @ hermes_cli/commands.py:resolve_command */
static json_t *cm3_resolve_command(const char *name, const json_t *registry) {
    char *q = cm3_query_key(name);
    json_t *ret = NULL;
    if (is_arr(registry)) {
        for (size_t i = 0; i < json_len(registry) && !ret; i++) {
            const json_t *cmd = json_get(registry, i);
            const char *cn = jstr(json_obj_get(cmd, "name"));
            if (cn && !strcmp(cn, q)) { ret = json_copy(cmd); break; }
            const json_t *al = json_obj_get(cmd, "aliases");
            if (is_arr(al)) {
                for (size_t j = 0; j < json_len(al); j++) {
                    const char *x = jstr(json_get(al, j));
                    if (x && !strcmp(x, q)) { ret = json_copy(cmd); break; }
                }
            }
        }
    }
    free(q);
    return ret;
}

/* PoP: cm3_build_description @ hermes_cli/commands.py:_build_description */
static json_t *cm3_build_description(const json_t *cmd, const cm3_seams_t *s) {
    const json_t *ah = json_obj_get(cmd, "args_hint");
    json_t *da = a0();
    aset_j(da, "cmd", cmd);
    json_t *d = cm3_call(s, "describe", da);
    json_free(da);
    if (CM3_RAISED(d)) { cm3_park(d); return NULL; }
    if (!jtruth(ah)) return d;
    json_t *a = a0();
    aset_s(a, "key", "slash.shared.usage_suffix");
    aset_j(a, "description", d);
    aset_s(a, "name", jstr(json_obj_get(cmd, "name")));
    aset_j(a, "args_hint", ah);
    json_t *r = cm3_call(s, "t", a);
    json_free(a);
    json_free(d);
    if (CM3_RAISED(r)) { cm3_park(r); return NULL; }
    return r;
}

/* PoP: cm3_is_gateway_known_command @ hermes_cli/commands.py:is_gateway_known_command */
static bool cm3_is_gateway_known_command(const char *name, const json_t *registry,
                                          const cm3_seams_t *s) {
    if (!name || !name[0]) return false;
    if (is_arr(registry)) {
        for (size_t i = 0; i < json_len(registry); i++) {
            const json_t *cmd = json_get(registry, i);
            bool cli_only = jtruth(json_obj_get(cmd, "cli_only"));
            bool gate = jtruth(json_obj_get(cmd, "gateway_config_gate"));
            if (cli_only && !gate) continue;
            const char *cn = jstr(json_obj_get(cmd, "name"));
            if (cn && !strcmp(cn, name)) return true;
            const json_t *al = json_obj_get(cmd, "aliases");
            if (is_arr(al)) {
                for (size_t j = 0; j < json_len(al); j++) {
                    const char *x = jstr(json_get(al, j));
                    if (x && !strcmp(x, name)) return true;
                }
            }
        }
    }
    json_t *r = cm3_call(s, "_iter_plugin_command_entries", a0());
    if (CM3_RAISED(r)) { cm3_park(r); return false; }
    bool found = false;
    if (is_arr(r)) {
        for (size_t i = 0; i < json_len(r) && !found; i++) {
            const json_t *trip = json_get(r, i);
            if (is_arr(trip) && json_len(trip) > 0) {
                const char *pn = jstr(json_get(trip, 0));
                if (pn && !strcmp(pn, name)) found = true;
            }
        }
    }
    json_free(r);
    return found;
}

/* PoP: cm3_is_interrupt_then_dispatch @ hermes_cli/commands.py:is_interrupt_then_dispatch */
static bool cm3_is_interrupt_then_dispatch(const char *name, const json_t *registry) {
    if (!name || !name[0]) return false;
    json_t *cmd = cm3_resolve_command(name, registry);
    if (!cmd) return false;
    const char *bp = jstr(json_obj_get(cmd, "busy_policy"));
    bool hit = bp && !strcmp(bp, "interrupt_then_dispatch");
    json_free(cmd);
    return hit;
}

/* PoP: cm3_should_bypass_active_session @ hermes_cli/commands.py:should_bypass_active_session */
static bool cm3_should_bypass_active_session(const char *name, const json_t *registry) {
    if (!name || !name[0]) return false;
    json_t *cmd = cm3_resolve_command(name, registry);
    if (!cmd) return false;
    json_free(cmd);
    return true;
}

/* PoP: cm3_resolve_config_gates @ hermes_cli/commands.py:_resolve_config_gates */
static json_t *cm3_resolve_config_gates(const json_t *registry, const json_t *inp,
                                         const cm3_seams_t *s) {
    json_t *out = json_array();
    json_t *gated = json_array();
    if (is_arr(registry)) {
        for (size_t i = 0; i < json_len(registry); i++) {
            const json_t *cmd = json_get(registry, i);
            if (jtruth(json_obj_get(cmd, "gateway_config_gate")))
                json_append(gated, json_copy(cmd));
        }
    }
    if (json_len(gated) == 0) { json_free(gated); return out; }
    /* `from hermes_cli.config import ...` sits INSIDE the try: gated -> swallowed. */
    if (cm3_import_gated(inp, "hermes_cli.config")) { json_free(gated); return out; }
    json_t *cfg = cm3_call(s, "read_raw_config", a0());
    if (CM3_RAISED(cfg)) {
        if (cm3_exc_is_exception(cfg)) { json_free(cfg); json_free(gated); return out; }
        cm3_park(cfg); json_free(gated); return NULL;
    }
    for (size_t i = 0; i < json_len(gated); i++) {
        const json_t *cmd = json_get(gated, i);
        const char *gp = jstr(json_obj_get(cmd, "gateway_config_gate"));
        json_t *parts = cm3_split_dots(gp);
        json_t *ca = a0();
        aset_j(ca, "cfg", cfg);
        aset_j(ca, "parts", parts);
        json_t *v = cm3_call(s, "cfg_get", ca);
        json_free(ca); json_free(parts);
        if (CM3_RAISED(v)) { cm3_park(v); json_free(cfg); json_free(gated); json_free(out); return NULL; }
        json_t *ta = a0();
        aset_j(ta, "value", v);
        aset_b(ta, "default", false);
        json_t *truth = cm3_call(s, "is_truthy_value", ta);
        json_free(ta); json_free(v);
        if (CM3_RAISED(truth)) { cm3_park(truth); json_free(cfg); json_free(gated); json_free(out); return NULL; }
        if (jtruth(truth)) {
            const char *nm = jstr(json_obj_get(cmd, "name"));
            if (nm && !cm3_arr_has_str(out, nm)) json_append(out, json_string(nm));
        }
        json_free(truth);
    }
    json_free(cfg); json_free(gated);
    cm3_sort_strs(out);
    return out;
}

/* helper for gateway_help_lines: join an array of strings with ", ". */
static char *cm3_join(const json_t *arr, const char *sep) {
    size_t cap = 1, n = is_arr(arr) ? json_len(arr) : 0;
    for (size_t i = 0; i < n; i++) {
        const char *s = jstr(json_get(arr, i));
        cap += s ? strlen(s) + strlen(sep) : 1;
    }
    char *out = malloc(cap + 1);
    size_t k = 0;
    out[0] = '\0';
    for (size_t i = 0; i < n; i++) {
        if (i) { size_t sl = strlen(sep); memcpy(out + k, sep, sl); k += sl; }
        const char *s = jstr(json_get(arr, i));
        size_t sl = s ? strlen(s) : 0;
        if (sl) { memcpy(out + k, s, sl); k += sl; }
    }
    out[k] = '\0';
    return out;
}

/* PoP: cm3_gateway_help_lines @ hermes_cli/commands.py:gateway_help_lines */
static json_t *cm3_gateway_help_lines(const json_t *allowed, const json_t *registry,
                                       const cm3_seams_t *s) {
    json_t *lines = json_array();
    json_t *ov = cm3_call(s, "_resolve_config_gates", a0());
    if (CM3_RAISED(ov)) { cm3_park(ov); return NULL; }
    if (is_arr(registry)) {
        for (size_t i = 0; i < json_len(registry); i++) {
            const json_t *cmd = json_get(registry, i);
            json_t *aa = a0();
            aset_j(aa, "cmd", cmd);
            aset_j(aa, "overrides", ov);
            json_t *av = cm3_call(s, "_is_gateway_available", aa);
            json_free(aa);
            if (CM3_RAISED(av)) { cm3_park(av); json_free(ov); json_free(lines); return NULL; }
            bool avail = jtruth(av);
            json_free(av);
            if (!avail) continue;
            const char *nm = jstr(json_obj_get(cmd, "name"));
            if (allowed && cm3_arr_has_str(allowed, nm) == false && nm) {
                /* allowed_set is a set; a null name is never in it (skip). */
                continue;
            }
            const char *ah = jstr(json_obj_get(cmd, "args_hint"));
            char *suffix = (ah && ah[0]) ? xsf(" %s", ah) : strdup("");
            json_t *alias_parts = json_array();
            const json_t *als = json_obj_get(cmd, "aliases");
            if (is_arr(als)) {
                for (size_t j = 0; j < json_len(als); j++) {
                    const char *al = jstr(json_get(als, j));
                    if (!al) continue;
                    char *an = cm3_dash2us(al);
                    char *nn = cm3_dash2us(nm);
                    bool internal = (!strcmp(an, nn) && strcmp(al, nm ? nm : ""));
                    free(an); free(nn);
                    if (!internal) {
                        char *part = xsf("`/%s`", al);
                        json_append(alias_parts, json_string(part));
                        free(part);
                    }
                }
            }
            json_t *note = NULL;
            if (json_len(alias_parts) > 0) {
                char *joined = cm3_join(alias_parts, ", ");
                json_t *ta = a0();
                aset_s(ta, "key", "slash.shared.alias_note");
                aset_s(ta, "aliases", joined);
                note = cm3_call(s, "t", ta);
                json_free(ta); free(joined);
                if (CM3_RAISED(note)) { cm3_park(note); json_free(alias_parts); free(suffix); json_free(ov); json_free(lines); return NULL; }
            } else {
                note = json_string("");
            }
            json_t *da = a0();
            aset_j(da, "cmd", cmd);
            json_t *d = cm3_call(s, "describe", da);
            json_free(da);
            if (CM3_RAISED(d)) { cm3_park(d); json_free(note); json_free(alias_parts); free(suffix); json_free(ov); json_free(lines); return NULL; }
            char *ds = cm3_pystr(d), *ns_ = cm3_pystr(note);
            char *line = xsf("`/%s%s` -- %s%s", nm ? nm : "", suffix, ds, ns_);
            json_append(lines, json_string(line));
            free(line); free(ds); free(ns_);
            json_free(d); json_free(note); json_free(alias_parts); free(suffix);
        }
    }
    json_free(ov);
    return lines;
}

/* PoP: cm3_iter_plugin_command_entries @ hermes_cli/commands.py:_iter_plugin_command_entries */
static json_t *cm3_iter_plugin_command_entries(const json_t *inp, const cm3_seams_t *s) {
    json_t *out = json_array();
    /* `from hermes_cli.plugins import ...` sits INSIDE the try. */
    if (cm3_import_gated(inp, "hermes_cli.plugins")) return out;
    json_t *cmds = cm3_call(s, "get_plugin_commands", a0());
    if (CM3_RAISED(cmds)) {
        if (cm3_exc_is_exception(cmds)) { json_free(cmds); return out; }
        cm3_park(cmds); return NULL;
    }
    if (!is_obj(cmds)) { json_free(cmds); return out; }  /* None / {} -> [] */
    for (size_t i = 0; i < cmds->c.count; i++) {
        const char *nm = cmds->c.keys[i];
        const json_t *meta = cmds->c.items[i];
        if (!is_obj(meta)) continue;  /* isinstance(meta, dict) */
        char *desc;
        const json_t *dv = json_obj_get(meta, "description");
        if (jtruth(dv)) {
            desc = cm3_pystr(dv);
        } else {
            json_t *ta = a0();
            aset_s(ta, "key", "slash.shared.plugin_default_desc");
            aset_s(ta, "name", nm);
            json_t *td = cm3_call(s, "t", ta);
            json_free(ta);
            if (CM3_RAISED(td)) { cm3_park(td); json_free(cmds); json_free(out); return NULL; }
            desc = cm3_pystr(td);
            json_free(td);
        }
        const json_t *hv = json_obj_get(meta, "args_hint");
        char *raw_hint = jtruth(hv) ? cm3_pystr(hv) : strdup("");
        char *hint = cm3_strip(raw_hint);
        free(raw_hint);
        json_t *trip = json_array();
        json_append(trip, json_string(nm));
        json_append(trip, json_string(desc));
        json_append(trip, json_string(hint));
        json_append(out, trip);
        free(desc); free(hint);
    }
    json_free(cmds);
    return out;
}
