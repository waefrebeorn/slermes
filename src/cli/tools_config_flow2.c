/* tools_config_flow2.c — FLOW-core port of the UNCLAIMED platform-toolsets
 * resolution/configuration remainder of hermes_cli/tools_config.py.
 *
 * Sibling of config_flow2.c / config_flow2b.c: same universal seam dispatcher.
 * Every collaborator / I/O call goes through tc2_seam(io, name, args); the
 * harness dispatcher appends `name` to io->trace, returns the scripted result,
 * applies "state" effects, and models a raised exception as
 * {"__raise__": {type, message, ...}}. Every seam except the two cosmetic ones
 * ("print" / "color") is SCRIPTED and FAIL-CLOSED: a missing script raises
 * RuntimeError("unscripted seam: <name>").
 *
 * Set-valued Python results are modeled as JSON arrays of strings, normalized
 * to sorted order on both sides (Python sets iterate arbitrarily). Fixture
 * domain is ASCII for user strings. Path values are verbatim posix strings.
 *
 * Ownership: seam results are caller-owned (json_free). io->trace / io->state
 * / cap_stdout / cap_stderr are harness-owned. tc2_seam() frees its args.
 * Every public tc2_* function returns a NEW reference (or a raise sentinel).
 *
 * CODE-ONLY: included by tests/t_port_tools_config_flow2.c; not registered in
 * build/objects.mk (scanner credit deliberately not chased).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* ── seam dispatcher + trace + state ───────────────────────────────────── */
typedef struct tc2_flow_io_t {
    json_t *(*call)(struct tc2_flow_io_t *io, const char *name, json_t *args);
    json_t *trace;
    json_t *state;
    json_t *cap_stdout;
    json_t *cap_stderr;
    void   *ctx;
} tc2_flow_io_t;

static void tc2_trace(tc2_flow_io_t *io, const char *token) {
    if (!io || !io->trace || io->trace->type != JSON_ARRAY) return;
    json_append(io->trace, json_string(token));
}
static json_t *tc2_call(tc2_flow_io_t *io, const char *name, json_t *args) {
    if (io && io->call) return io->call(io, name, args);
    return json_null();
}
static bool tc2_raised(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}
static json_t *tc2_raise_spec(const json_t *r) {
    return tc2_raised(r) ? json_obj_get(r, "__raise__") : NULL;
}
static json_t *tc2_make_raise(const char *type, const char *message) {
    json_t *spec = json_object();
    json_set(spec, "type", json_string(type));
    json_set(spec, "message", json_string(message ? message : ""));
    json_t *r = json_object();
    json_set(r, "__raise__", spec);
    return r;
}
/* seam: trace, dispatch, free args. */
static json_t *tc2_seam(tc2_flow_io_t *io, const char *name, json_t *args) {
    tc2_trace(io, name);
    json_t *r = tc2_call(io, name, args);
    if (args) json_free(args);
    if (!r) return json_null();
    return r;
}

/* ── Python-faithful helpers ───────────────────────────────────────────── */
static bool tc2_truthy(const json_t *v) {
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
static bool tc2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *tc2_sval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static char *tc2_join(const char *a, const char *b) {
    size_t na = strlen(a ? a : ""), nb = strlen(b ? b : "");
    char *out = malloc(na + nb + 1);
    if (!out) return NULL;
    if (na) memcpy(out, a, na);
    if (nb) memcpy(out + na, b, nb);
    out[na + nb] = '\0';
    return out;
}
static char *tc2_strip(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p), a = 0, b = n;
    while (a < b && isspace((unsigned char)p[a])) a++;
    while (b > a && isspace((unsigned char)p[b - 1])) b--;
    char *out = malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, p + a, b - a);
    out[b - a] = '\0';
    return out;
}
static char *tc2_lower(const char *s) {
    char *out = strdup(s ? s : "");
    for (char *p = out; p && *p; p++) *p = (char)tolower((unsigned char)*p);
    return out;
}
static json_t *tc2_state_get(tc2_flow_io_t *io, const char *key) {
    if (!io || !io->state || io->state->type != JSON_OBJECT) return NULL;
    return json_obj_get(io->state, key);
}
static void tc2_state_add(tc2_flow_io_t *io, const char *key, const char *val) {
    json_t *arr = tc2_state_get(io, key);
    if (!arr || arr->type != JSON_ARRAY) {
        arr = json_array();
        json_set(io->state, key, arr);
    }
    bool present = false;
    for (size_t i = 0; i < json_len(arr); i++)
        if (strcmp(tc2_sval(json_get(arr, i)), val) == 0) { present = true; break; }
    if (!present) json_append(arr, json_string(val));
}

/* ── sorted JSON-array-of-strings set helpers ──────────────────────────── */
static bool tc2_has(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < json_len(arr); i++)
        if (strcmp(tc2_sval(json_get(arr, i)), s) == 0) return true;
    return false;
}
static void tc2_add(json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY || tc2_has(arr, s)) return;
    json_append(arr, json_string(s));
}
static void tc2_remove(json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY) return;
    for (size_t i = 0; i < arr->c.count; i++) {
        if (strcmp(tc2_sval(json_get(arr, i)), s) == 0) {
            json_free(arr->c.items[i]);
            for (size_t j = i; j + 1 < arr->c.count; j++) arr->c.items[j] = arr->c.items[j + 1];
            arr->c.count--;
            return;
        }
    }
}
static json_t *tc2_sorted(const json_t *arr) {
    json_t *out = json_array();
    size_t n = (arr && arr->type == JSON_ARRAY) ? json_len(arr) : 0;
    for (size_t i = 0; i < n; i++) {
        const char *v = tc2_sval(json_get(arr, i));
        if (tc2_has(out, v)) continue;
        size_t k = json_len(out), j = k;
        while (j > 0 && strcmp(tc2_sval(json_get(out, j - 1)), v) > 0) j--;
        json_t *node = json_string(v);
        json_append(out, json_null());
        json_free(out->c.items[json_len(out) - 1]);
        for (size_t m = json_len(out) - 1; m > j; m--) out->c.items[m] = out->c.items[m - 1];
        out->c.items[j] = node;
    }
    return out;
}
static json_t *tc2_diff(const json_t *a, const json_t *b) {
    json_t *out = json_array();
    size_t n = (a && a->type == JSON_ARRAY) ? json_len(a) : 0;
    for (size_t i = 0; i < n; i++) {
        const char *v = tc2_sval(json_get(a, i));
        if (!tc2_has(b, v)) tc2_add(out, v);
    }
    return tc2_sorted(out);
}
static json_t *tc2_intersect(const json_t *a, const json_t *b) {
    json_t *out = json_array();
    size_t n = (a && a->type == JSON_ARRAY) ? json_len(a) : 0;
    for (size_t i = 0; i < n; i++) {
        const char *v = tc2_sval(json_get(a, i));
        if (tc2_has(b, v)) tc2_add(out, v);
    }
    return tc2_sorted(out);
}
static json_t *tc2_union(const json_t *a, const json_t *b) {
    json_t *out = tc2_sorted(a);
    size_t n = (b && b->type == JSON_ARRAY) ? json_len(b) : 0;
    for (size_t i = 0; i < n; i++) tc2_add(out, tc2_sval(json_get(b, i)));
    return tc2_sorted(out);
}
static bool tc2_subset(const json_t *a, const json_t *b) {
    size_t n = (a && a->type == JSON_ARRAY) ? json_len(a) : 0;
    for (size_t i = 0; i < n; i++) if (!tc2_has(b, tc2_sval(json_get(a, i)))) return false;
    return true;
}

/* ── full builtin exception hierarchy (for `except Exception` families) ─ */
typedef struct { const char *name; const char *parent; } tc2_exc_t;
static const tc2_exc_t TC2_EXC_TABLE[] = {
    {"BaseException", NULL},
    {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"}, {"Exception", "BaseException"},
    {"TypeError", "Exception"}, {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"}, {"ArithmeticError", "Exception"},
    {"FloatingPointError", "ArithmeticError"}, {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"}, {"AssertionError", "Exception"},
    {"AttributeError", "Exception"}, {"BufferError", "Exception"},
    {"EOFError", "Exception"}, {"ImportError", "Exception"},
    {"ModuleNotFoundError", "ImportError"}, {"LookupError", "Exception"},
    {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
    {"MemoryError", "Exception"}, {"NameError", "Exception"},
    {"UnboundLocalError", "NameError"}, {"OSError", "Exception"},
    {"BlockingIOError", "OSError"}, {"ChildProcessError", "OSError"},
    {"ConnectionError", "OSError"}, {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"}, {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"}, {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"}, {"NotADirectoryError", "OSError"},
    {"PermissionError", "OSError"}, {"ProcessLookupError", "OSError"},
    {"TimeoutError", "OSError"}, {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"}, {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"}, {"SyntaxError", "Exception"},
    {"IndentationError", "SyntaxError"}, {"TabError", "IndentationError"},
    {"SystemError", "Exception"}, {"ValueError", "Exception"},
    {"UnicodeError", "ValueError"}, {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"}, {"UnicodeTranslateError", "UnicodeError"},
    {"Warning", "Exception"}, {"DeprecationWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"}, {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"}, {"UserWarning", "Warning"},
    {"FutureWarning", "Warning"}, {"ImportWarning", "Warning"},
    {"UnicodeWarning", "Warning"}, {"BytesWarning", "Warning"},
    {"ResourceWarning", "Warning"}, {"EncodingWarning", "Warning"},
    {NULL, NULL},
};
static const char *tc2_exc_parent(const char *type) {
    if (!type) return NULL;
    for (int i = 0; TC2_EXC_TABLE[i].name; i++)
        if (strcmp(type, TC2_EXC_TABLE[i].name) == 0) return TC2_EXC_TABLE[i].parent;
    return NULL;
}
static bool tc2_exc_is_exception(const char *type) {
    const char *cur = type;
    for (int depth = 0; cur && depth < 32; depth++) {
        if (strcmp(cur, "Exception") == 0) return true;
        if (strcmp(cur, "BaseException") == 0) return false;
        cur = tc2_exc_parent(cur);
    }
    return false;
}

/* cosmetic AUTO seams */
static json_t *tc2_print(tc2_flow_io_t *io, const char *text, bool to_stderr) {
    json_t *args = json_object();
    json_set(args, "text", json_string(text ? text : ""));
    json_set(args, "file", to_stderr ? json_string("stderr") : json_null());
    return tc2_seam(io, "print", args);
}
static json_t *tc2_color(tc2_flow_io_t *io, const char *text, const char *code) {
    json_t *args = json_object();
    json_set(args, "text", json_string(text ? text : ""));
    json_set(args, "codes", json_array());
    if (code) json_append(json_obj_get(args, "codes"), json_string(code));
    return tc2_seam(io, "color", args);
}

/* forward declarations (composed siblings) */
static json_t *tc2_coerce(tc2_flow_io_t *io, json_t *value, const char *platform);
static json_t *tc2_configurable_subset_of(tc2_flow_io_t *io, json_t *tool_names, const char *platform, json_t *constants);
static json_t *tc2_default_off_toolsets(tc2_flow_io_t *io, const char *platform, bool explicit, json_t *constants);
static json_t *tc2_toolset_label(tc2_flow_io_t *io, const char *ts_key);
static json_t *tc2_toolsets_needing_setup(tc2_flow_io_t *io, json_t *new_enabled, json_t *config);

/* ==================================================================== */
/* 1. _coerce_platform_toolsets_value                                     */
/* ==================================================================== */
/* PoP: tc2_coerce @ hermes_cli/tools_config.py:_coerce_platform_toolsets_value */
static json_t *tc2_coerce(tc2_flow_io_t *io, json_t *value, const char *platform) {
    if (tc2_is_null(value)) return json_null();
    json_t *a = json_object(); json_set(a, "value", json_copy(value));
    json_t *parsed = tc2_seam(io, "parse_toolsets_value", a);
    if (tc2_raised(parsed)) return parsed;
    if (parsed && parsed->type != JSON_NULL) return parsed;
    json_free(parsed);
    json_t *warned = tc2_state_get(io, "warned");
    if (!warned || !tc2_has(warned, platform)) {
        tc2_state_add(io, "warned", platform);
        json_t *wa = json_object();
        json_set(wa, "fmt", json_string(
            "platform_toolsets.%s is %r, expected a YAML list of toolset names "
            "(e.g. [terminal, file, web]) - falling back to the platform default. "
            "Run `hermes tools` to reconfigure."));
        json_t *args = json_array();
        json_append(args, json_string(platform ? platform : ""));
        json_append(args, json_copy(value));
        json_set(wa, "args", args);
        json_t *r = tc2_seam(io, "logger.warning", wa);
        if (tc2_raised(r)) return r;
        json_free(r);
    }
    return json_copy(value);
}

/* ==================================================================== */
/* 2. _platform_toolsets_explicitly_saved                                 */
/* ==================================================================== */
/* PoP: tc2_platform_toolsets_explicitly_saved @ hermes_cli/tools_config.py:_platform_toolsets_explicitly_saved */
json_t *tc2_platform_toolsets_explicitly_saved(tc2_flow_io_t *io, json_t *config, const char *platform) {
    json_t *pts = json_obj_get(config, "platform_toolsets");
    json_t *raw = (pts && pts->type == JSON_OBJECT) ? json_obj_get(pts, platform) : NULL;
    json_t *v = tc2_coerce(io, raw, platform);
    if (tc2_raised(v)) return v;
    bool islist = v && v->type == JSON_ARRAY;
    json_free(v);
    return json_bool(islist);
}

/* ==================================================================== */
/* 3. _default_off_toolsets                                               */
/* ==================================================================== */
/* PoP: tc2_default_off_toolsets @ hermes_cli/tools_config.py:_default_off_toolsets */
static json_t *tc2_default_off_toolsets(tc2_flow_io_t *io, const char *platform, bool explicit, json_t *constants) {
    (void)io;
    json_t *src = json_obj_get(constants, "default_off_toolsets");
    json_t *out = tc2_sorted(src);
    json_t *restr = json_obj_get(constants, "toolset_platform_restrictions");
    bool platform_is_restr = restr && restr->type == JSON_OBJECT && json_obj_get(restr, platform);
    if (tc2_has(out, platform) && !platform_is_restr) tc2_remove(out, platform);
    if (explicit && restr && restr->type == JSON_OBJECT) {
        json_t *todel = json_array();
        for (size_t i = 0; i < json_len(out); i++) {
            const char *ts = tc2_sval(json_get(out, i));
            json_t *pl = json_obj_get(restr, ts);
            if (pl && pl->type == JSON_ARRAY && tc2_has(pl, platform)) json_append(todel, json_string(ts));
        }
        for (size_t i = 0; i < json_len(todel); i++) tc2_remove(out, tc2_sval(json_get(todel, i)));
        json_free(todel);
    }
    return tc2_sorted(out);
}

/* ==================================================================== */
/* 4. _configurable_subset_of                                             */
/* ==================================================================== */
/* PoP: tc2_configurable_subset_of @ hermes_cli/tools_config.py:_configurable_subset_of */
static json_t *tc2_configurable_subset_of(tc2_flow_io_t *io, json_t *tool_names, const char *platform, json_t *constants) {
    json_t *out = json_array();
    json_t *cfg = json_obj_get(constants, "configurable_toolsets");
    size_t n = (cfg && cfg->type == JSON_ARRAY) ? json_len(cfg) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *tr = json_get(cfg, i);
        const char *ts = tc2_sval(json_get(tr, 0));
        json_t *a = json_object();
        json_set(a, "ts_key", json_string(ts));
        json_set(a, "platform", json_string(platform ? platform : ""));
        json_t *allowed = tc2_seam(io, "toolset_allowed_for_platform", a);
        if (tc2_raised(allowed)) { json_free(out); return allowed; }
        bool ok = tc2_truthy(allowed); json_free(allowed);
        if (!ok) continue;
        json_t *ra = json_object();
        json_set(ra, "name", json_string(ts));
        json_set(ra, "include_registry", json_bool(false));
        json_t *tools = tc2_seam(io, "resolve_toolset", ra);
        if (tc2_raised(tools)) { json_free(out); return tools; }
        json_t *stools = tc2_sorted(tools);
        json_free(tools);
        if (json_len(stools) > 0 && tc2_subset(stools, tool_names)) tc2_add(out, ts);
        json_free(stools);
    }
    return tc2_sorted(out);
}

/* ==================================================================== */
/* 5. _composite_toolsets                                                 */
/* ==================================================================== */
/* PoP: tc2_composite_toolsets @ hermes_cli/tools_config.py:_composite_toolsets */
json_t *tc2_composite_toolsets(tc2_flow_io_t *io, json_t *toolset_names, const char *platform,
                               bool explicit, json_t *constants) {
    json_t *all = json_array();
    size_t n = (toolset_names && toolset_names->type == JSON_ARRAY) ? json_len(toolset_names) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *a = json_object();
        json_set(a, "name", json_string(tc2_sval(json_get(toolset_names, i))));
        json_set(a, "include_registry", json_bool(true));
        json_t *r = tc2_seam(io, "resolve_toolset", a);
        if (tc2_raised(r)) { json_free(all); return r; }
        json_t *sr = tc2_sorted(r); json_free(r);
        for (size_t j = 0; j < json_len(sr); j++) tc2_add(all, tc2_sval(json_get(sr, j)));
        json_free(sr);
    }
    json_t *subset = tc2_configurable_subset_of(io, all, platform, constants);
    json_free(all);
    if (tc2_raised(subset)) return subset;
    json_t *default_off = tc2_default_off_toolsets(io, platform, explicit, constants);
    json_t *a = json_object();
    json_set(a, "ts_key", json_string("x_search"));
    json_set(a, "platform", json_string(platform ? platform : ""));
    json_t *allowed = tc2_seam(io, "toolset_allowed_for_platform", a);
    if (tc2_raised(allowed)) { json_free(subset); json_free(default_off); return allowed; }
    bool ok = tc2_truthy(allowed); json_free(allowed);
    if (ok) {
        json_t *xai = tc2_seam(io, "xai_credentials_present", NULL);
        if (tc2_raised(xai)) { json_free(subset); json_free(default_off); return xai; }
        bool yes = tc2_truthy(xai); json_free(xai);
        if (yes) { tc2_add(subset, "x_search"); tc2_remove(default_off, "x_search"); }
    }
    json_t *out = tc2_diff(subset, default_off);
    json_free(subset); json_free(default_off);
    return out;
}

/* ==================================================================== */
/* 6. _enabled_plugin_toolsets                                            */
/* ==================================================================== */
/* PoP: tc2_enabled_plugin_toolsets @ hermes_cli/tools_config.py:_enabled_plugin_toolsets */
json_t *tc2_enabled_plugin_toolsets(tc2_flow_io_t *io, json_t *config, const char *platform,
                                    json_t *toolset_names, json_t *plugin_ts_keys, json_t *constants) {
    (void)io;
    json_t *out = json_array();
    json_t *known = json_array();
    json_t *kpts = json_obj_get(config, "known_plugin_toolsets");
    if (kpts && kpts->type == JSON_OBJECT) {
        json_t *kp = json_obj_get(kpts, platform);
        if (kp && kp->type == JSON_ARRAY)
            for (size_t i = 0; i < json_len(kp); i++) tc2_add(known, tc2_sval(json_get(kp, i)));
    }
    json_t *doff = json_obj_get(constants, "default_off_toolsets");
    size_t n = (plugin_ts_keys && plugin_ts_keys->type == JSON_ARRAY) ? json_len(plugin_ts_keys) : 0;
    for (size_t i = 0; i < n; i++) {
        const char *pts = tc2_sval(json_get(plugin_ts_keys, i));
        if (tc2_has(toolset_names, pts) ||
            (!tc2_has(doff, pts) && !tc2_has(known, pts))) tc2_add(out, pts);
    }
    json_free(known);
    return tc2_sorted(out);
}

/* ==================================================================== */
/* 7. _context_engine_active                                              */
/* ==================================================================== */
/* PoP: tc2_context_engine_active @ hermes_cli/tools_config.py:_context_engine_active */
json_t *tc2_context_engine_active(tc2_flow_io_t *io, json_t *config) {
    (void)io;
    const char *name = "compressor";
    char *owned = NULL;
    json_t *ctx = json_obj_get(config, "context");
    if (ctx && ctx->type == JSON_OBJECT) {
        json_t *eng = json_obj_get(ctx, "engine");
        if (eng && !tc2_is_null(eng) && eng->type == JSON_STRING)
            owned = tc2_strip(eng->str_val);
    }
    if (owned) name = owned;
    char *lower = tc2_lower(name);
    bool active = lower && lower[0] != '\0' && strcmp(lower, "compressor") != 0;
    free(lower); free(owned);
    return json_bool(active);
}

/* ==================================================================== */
/* 8. _merge_mcp_servers                                                  */
/* ==================================================================== */
/* PoP: tc2_merge_mcp_servers @ hermes_cli/tools_config.py:_merge_mcp_servers */
json_t *tc2_merge_mcp_servers(tc2_flow_io_t *io, json_t *config, json_t *toolset_names,
                              json_t *explicit_passthrough, bool include_default) {
    json_t *ma = json_object(); json_set(ma, "config", json_copy(config));
    json_t *ev = tc2_seam(io, "enabled_mcp_server_names", ma);
    if (tc2_raised(ev)) return ev;
    json_t *enabled = tc2_sorted(ev); json_free(ev);
    json_t *known = tc2_sorted(enabled);
    json_t *configured = json_obj_get(config, "mcp_servers");
    if (configured && configured->type == JSON_OBJECT)
        for (size_t i = 0; i < configured->c.count; i++)
            tc2_add(known, configured->c.keys[i]);
    json_t *pass = json_array();
    size_t n = (explicit_passthrough && explicit_passthrough->type == JSON_ARRAY)
               ? json_len(explicit_passthrough) : 0;
    for (size_t i = 0; i < n; i++) {
        const char *ts = tc2_sval(json_get(explicit_passthrough, i));
        if (strncmp(ts, "mcp-", 4) == 0 && !tc2_has(known, ts) && tc2_has(known, ts + 4))
            tc2_add(pass, ts + 4);
        else
            tc2_add(pass, ts);
    }
    json_t *result = tc2_diff(pass, known);
    if (tc2_has(toolset_names, "no_mcp")) {
        json_t *dummy = json_array(); json_append(dummy, json_string("no_mcp"));
        json_t *out = tc2_diff(result, dummy);
        json_free(dummy);
        json_free(known); json_free(pass); json_free(result); json_free(enabled);
        return out;
    }
    json_t *explicit_mcp = tc2_intersect(pass, enabled);
    json_t *pass_known = tc2_intersect(pass, known);
    json_t *out;
    if (include_default && json_len(pass_known) == 0)
        out = tc2_union(result, enabled);
    else
        out = tc2_union(result, explicit_mcp);
    json_free(pass_known); json_free(explicit_mcp);
    json_free(known); json_free(pass); json_free(result); json_free(enabled);
    return out;
}

/* ==================================================================== */
/* 9. _prune_toolsets_stripped_by_disabled                                */
/* ==================================================================== */
/* PoP: tc2_prune_toolsets_stripped_by_disabled @ hermes_cli/tools_config.py:_prune_toolsets_stripped_by_disabled */
json_t *tc2_prune_toolsets_stripped_by_disabled(tc2_flow_io_t *io, json_t *enabled, json_t *disabled) {
    json_t *remaining = tc2_diff(enabled, disabled);
    json_t *resolved = json_object();
    for (size_t i = 0; i < json_len(remaining); i++) {
        const char *name = tc2_sval(json_get(remaining, i));
        json_t *va = json_object(); json_set(va, "name", json_string(name));
        json_t *v = tc2_seam(io, "validate_toolset", va);
        if (tc2_raised(v)) { json_free(remaining); json_free(resolved); return v; }
        bool valid = tc2_truthy(v); json_free(v);
        json_t *tools = json_array();
        if (valid) {
            json_t *ra = json_object();
            json_set(ra, "name", json_string(name));
            json_set(ra, "include_registry", json_bool(true));
            json_t *r = tc2_seam(io, "resolve_toolset", ra);
            if (tc2_raised(r)) { json_free(remaining); json_free(resolved); return r; }
            json_t *sr = tc2_sorted(r); json_free(r);
            json_free(tools); tools = sr;
        }
        json_set(resolved, name, tools);
    }
    json_t *surviving = json_array();
    for (size_t i = 0; i < resolved->c.count; i++) {
        json_t *tools = resolved->c.items[i];
        for (size_t j = 0; j < json_len(tools); j++) tc2_add(surviving, tc2_sval(json_get(tools, j)));
    }
    json_t *sa = json_object();
    json_set(sa, "tools", json_copy(surviving));
    json_set(sa, "disabled", json_copy(disabled));
    json_set(sa, "quiet", json_bool(true));
    json_set(sa, "disable", json_bool(true));
    json_t *r = tc2_seam(io, "apply_toolset_selection", sa);
    if (tc2_raised(r)) { json_free(remaining); json_free(resolved); json_free(surviving); return r; }
    json_t *kept = tc2_sorted(r); json_free(r);
    json_t *out = json_array();
    for (size_t i = 0; i < json_len(remaining); i++) {
        const char *name = tc2_sval(json_get(remaining, i));
        json_t *tools = json_obj_get(resolved, name);
        bool keep = !tools || json_len(tools) == 0 ||
                    json_len(tc2_intersect(tools, kept)) > 0;
        if (keep) tc2_add(out, name);
    }
    json_free(kept); json_free(surviving);
    json_free(resolved); json_free(remaining);
    return tc2_sorted(out);
}

/* ==================================================================== */
/* 10. _disabled_entry_tools                                              */
/* ==================================================================== */
/* PoP: tc2_disabled_entry_tools @ hermes_cli/tools_config.py:_disabled_entry_tools */
json_t *tc2_disabled_entry_tools(tc2_flow_io_t *io, const char *name) {
    json_t *d = json_array(); json_append(d, json_string(name ? name : ""));
    json_t *a1 = json_object();
    json_set(a1, "tools", json_array());
    json_set(a1, "disabled", json_copy(d));
    json_set(a1, "quiet", json_bool(true));
    json_set(a1, "disable", json_bool(false));
    json_t *r1 = tc2_seam(io, "apply_toolset_selection", a1);
    if (tc2_raised(r1)) { json_free(d); return r1; }
    json_t *universe = tc2_sorted(r1); json_free(r1);
    json_t *a2 = json_object();
    json_set(a2, "tools", json_copy(universe));
    json_set(a2, "disabled", json_copy(d));
    json_set(a2, "quiet", json_bool(true));
    json_set(a2, "disable", json_bool(true));
    json_free(d);
    json_t *r2 = tc2_seam(io, "apply_toolset_selection", a2);
    if (tc2_raised(r2)) { json_free(universe); return r2; }
    json_t *kept = tc2_sorted(r2); json_free(r2);
    json_t *out = tc2_diff(universe, kept);
    json_free(kept); json_free(universe);
    return out;
}

/* ==================================================================== */
/* 11. _platform_default_toolset                                          */
/* ==================================================================== */
/* PoP: tc2_platform_default_toolset @ hermes_cli/tools_config.py:_platform_default_toolset */
json_t *tc2_platform_default_toolset(tc2_flow_io_t *io, const char *platform, json_t *constants) {
    (void)io;
    json_t *platforms = json_obj_get(constants, "platforms");
    if (platforms && platforms->type == JSON_OBJECT) {
        json_t *pi = json_obj_get(platforms, platform);
        if (pi && pi->type == JSON_OBJECT) {
            json_t *d = json_obj_get(pi, "default_toolset");
            return d ? json_copy(d) : json_null();
        }
    }
    char *s = tc2_join("hermes-", platform ? platform : "");
    json_t *out = json_string(s); free(s);
    return out;
}

/* ==================================================================== */
/* 12. _is_configurable                                                   */
/* ==================================================================== */
/* PoP: tc2_is_configurable @ hermes_cli/tools_config.py:_is_configurable */
json_t *tc2_is_configurable(tc2_flow_io_t *io, const char *ts_key, json_t *constants) {
    (void)io;
    json_t *cats = json_obj_get(constants, "tool_categories");
    json_t *env = json_obj_get(constants, "toolset_env_requirements");
    json_t *c = (cats && cats->type == JSON_OBJECT) ? json_obj_get(cats, ts_key) : NULL;
    json_t *e = (env && env->type == JSON_OBJECT) ? json_obj_get(env, ts_key) : NULL;
    return json_bool(tc2_truthy(c) || tc2_truthy(e));
}

/* ==================================================================== */
/* 13. _toolset_label                                                     */
/* ==================================================================== */
/* PoP: tc2_toolset_label @ hermes_cli/tools_config.py:_toolset_label */
static json_t *tc2_toolset_label(tc2_flow_io_t *io, const char *ts_key) {
    json_t *r = tc2_seam(io, "get_effective_configurable_toolsets", NULL);
    if (tc2_raised(r)) return r;
    size_t n = (r && r->type == JSON_ARRAY) ? json_len(r) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *tr = json_get(r, i);
        if (strcmp(tc2_sval(json_get(tr, 0)), ts_key) == 0) {
            json_t *out = json_copy(json_get(tr, 1));
            json_free(r);
            return out;
        }
    }
    json_free(r);
    return json_string(ts_key ? ts_key : "");
}

/* ==================================================================== */
/* 14. _configurable_keys                                                 */
/* ==================================================================== */
/* PoP: tc2_configurable_keys @ hermes_cli/tools_config.py:_configurable_keys */
json_t *tc2_configurable_keys(tc2_flow_io_t *io, json_t *constants) {
    (void)io;
    json_t *out = json_array();
    json_t *cfg = json_obj_get(constants, "configurable_toolsets");
    size_t n = (cfg && cfg->type == JSON_ARRAY) ? json_len(cfg) : 0;
    for (size_t i = 0; i < n; i++) tc2_add(out, tc2_sval(json_get(json_get(cfg, i), 0)));
    return tc2_sorted(out);
}

/* ==================================================================== */
/* 15. _platform_default_keys                                             */
/* ==================================================================== */
/* PoP: tc2_platform_default_keys @ hermes_cli/tools_config.py:_platform_default_keys */
json_t *tc2_platform_default_keys(tc2_flow_io_t *io, json_t *constants) {
    (void)io;
    json_t *out = json_array();
    json_t *platforms = json_obj_get(constants, "platforms");
    if (platforms && platforms->type == JSON_OBJECT)
        for (size_t i = 0; i < platforms->c.count; i++) {
            json_t *pi = platforms->c.items[i];
            if (pi && pi->type == JSON_OBJECT)
                tc2_add(out, tc2_sval(json_obj_get(pi, "default_toolset")));
        }
    return tc2_sorted(out);
}

/* ==================================================================== */
/* 16. _provider_env_ready                                                */
/* ==================================================================== */
/* PoP: tc2_provider_env_ready @ hermes_cli/tools_config.py:_provider_env_ready */
json_t *tc2_provider_env_ready(tc2_flow_io_t *io, json_t *provider) {
    json_t *env = json_obj_get(provider, "env_vars");
    size_t n = (env && env->type == JSON_ARRAY) ? json_len(env) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *a = json_object();
        json_set(a, "name", json_string(tc2_sval(json_obj_get(json_get(env, i), "key"))));
        json_t *r = tc2_seam(io, "get_env_value", a);
        if (tc2_raised(r)) return r;
        bool ok = tc2_truthy(r); json_free(r);
        if (!ok) return json_bool(false);
    }
    return json_bool(true);
}

/* ==================================================================== */
/* 17. _projected_plugin_toolsets                                         */
/* ==================================================================== */
/* PoP: tc2_projected_plugin_toolsets @ hermes_cli/tools_config.py:_projected_plugin_toolsets */
json_t *tc2_projected_plugin_toolsets(tc2_flow_io_t *io) {
    json_t *has = tc2_state_get(io, "gateway_provider");
    if (!tc2_truthy(has)) return json_null();
    return tc2_seam(io, "gateway_provider", NULL);
}

/* ==================================================================== */
/* 18. _print_toolset_diff                                                */
/* ==================================================================== */
/* PoP: tc2_print_toolset_diff @ hermes_cli/tools_config.py:_print_toolset_diff */
json_t *tc2_print_toolset_diff(tc2_flow_io_t *io, json_t *added, json_t *removed, const char *indent) {
    json_t *add = tc2_sorted(added), *rem = tc2_sorted(removed);
    for (int phase = 0; phase < 2; phase++) {
        json_t *set = phase == 0 ? add : rem;
        const char *sign = phase == 0 ? "+ " : "- ";
        const char *code = phase == 0 ? "GREEN" : "RED";
        for (size_t i = 0; i < json_len(set); i++) {
            json_t *lab = tc2_toolset_label(io, tc2_sval(json_get(set, i)));
            if (tc2_raised(lab)) { json_free(add); json_free(rem); return lab; }
            char *line = tc2_join(indent ? indent : "", sign);
            char *line2 = tc2_join(line, tc2_sval(lab)); free(line); json_free(lab);
            json_t *col = tc2_color(io, line2, code); free(line2);
            if (tc2_raised(col)) { json_free(add); json_free(rem); return col; }
            json_t *p = tc2_print(io, tc2_sval(col), false);
            json_free(col);
            if (tc2_raised(p)) { json_free(add); json_free(rem); return p; }
            json_free(p);
        }
    }
    json_free(add); json_free(rem);
    return json_null();
}

/* ==================================================================== */
/* 19. _checklist_diff                                                    */
/* ==================================================================== */
/* PoP: tc2_checklist_diff @ hermes_cli/tools_config.py:_checklist_diff */
json_t *tc2_checklist_diff(tc2_flow_io_t *io, json_t *new_enabled, json_t *prev, const char *platform) {
    json_t *a = json_object(); json_set(a, "platform", json_string(platform ? platform : ""));
    json_t *r = tc2_seam(io, "checklist_toolset_keys", a);
    if (tc2_raised(r)) return r;
    json_t *universe = tc2_sorted(r); json_free(r);
    json_t *nset = tc2_sorted(new_enabled), *pset = tc2_sorted(prev);
    json_t *added = tc2_intersect(tc2_diff(nset, pset), universe);
    json_t *removed = tc2_intersect(tc2_diff(pset, nset), universe);
    json_t *out = json_array();
    json_append(out, added); json_append(out, removed);
    json_free(universe); json_free(nset); json_free(pset);
    return out;
}

/* ==================================================================== */
/* 20. _current_platform_tools                                            */
/* ==================================================================== */
/* PoP: tc2_current_platform_tools @ hermes_cli/tools_config.py:_current_platform_tools */
json_t *tc2_current_platform_tools(tc2_flow_io_t *io, json_t *config, const char *pkey) {
    json_t *a = json_object();
    json_set(a, "config", json_copy(config));
    json_set(a, "pkey", json_string(pkey ? pkey : ""));
    json_set(a, "include_default_mcp_servers", json_bool(false));
    json_t *r = tc2_seam(io, "get_platform_tools", a);
    if (tc2_raised(r)) return r;
    json_t *out = tc2_sorted(r); json_free(r);
    return out;
}

/* ==================================================================== */
/* 21. _toolsets_needing_setup                                            */
/* ==================================================================== */
/* PoP: tc2_toolsets_needing_setup @ hermes_cli/tools_config.py:_toolsets_needing_setup */
static json_t *tc2_toolsets_needing_setup(tc2_flow_io_t *io, json_t *new_enabled, json_t *config) {
    json_t *out = json_array();
    json_t *sorted = tc2_sorted(new_enabled);
    for (size_t i = 0; i < json_len(sorted); i++) {
        const char *ts = tc2_sval(json_get(sorted, i));
        json_t *a = json_object(); json_set(a, "ts_key", json_string(ts));
        json_t *cfg = tc2_seam(io, "is_configurable", a);
        if (tc2_raised(cfg)) { json_free(sorted); json_free(out); return cfg; }
        bool ok = tc2_truthy(cfg); json_free(cfg);
        if (!ok) continue;
        json_t *pa = json_object();
        json_set(pa, "ts_key", json_string(ts));
        json_set(pa, "config", json_copy(config));
        json_set(pa, "force_fresh", json_bool(true));
        json_t *pr = tc2_seam(io, "toolset_needs_configuration_prompt", pa);
        if (tc2_raised(pr)) { json_free(sorted); json_free(out); return pr; }
        bool need = tc2_truthy(pr); json_free(pr);
        if (need) tc2_add(out, ts);
    }
    json_free(sorted);
    return tc2_sorted(out);
}

/* ==================================================================== */
/* 22. _configure_newly_added                                             */
/* ==================================================================== */
/* PoP: tc2_configure_newly_added @ hermes_cli/tools_config.py:_configure_newly_added */
json_t *tc2_configure_newly_added(tc2_flow_io_t *io, json_t *added, json_t *already, json_t *config) {
    json_t *d = tc2_diff(added, already);
    json_t *need = tc2_toolsets_needing_setup(io, d, config);
    json_free(d);
    if (tc2_raised(need)) return need;
    for (size_t i = 0; i < json_len(need); i++) {
        json_t *a = json_object();
        json_set(a, "ts_key", json_string(tc2_sval(json_get(need, i))));
        json_set(a, "config", json_copy(config));
        json_t *r = tc2_seam(io, "configure_toolset", a);
        if (tc2_raised(r)) { json_free(need); return r; }
        json_free(r);
    }
    json_free(need);
    return json_null();
}
