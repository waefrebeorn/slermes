/*
 * tui_methods_tools_flow.c — FLOW + leaf port of tui_gateway/methods_tools.py
 * (hermes-agent): the Plugins-hub RPC bodies (plugins.manage actions), the
 * shared dispatch/activation helpers and the small pure leaves they compose.
 *
 * Ports the DECISION SEQUENCES — branch order, error-code/message composition,
 * seam call order, module-global state transitions and exception paths — not
 * just leaf outputs. Every live external call enters through ONE universal
 * seam: tmt_call(ctx, name, args). The harness wires it to a recording stub
 * fed the case's scripted results (in.io); the oracle installs recording
 * stubs emitting the identical event shape {"name","args","ret"} from the
 * SAME scripts.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and tmt_-prefixed.
 *
 * Server globals are seams: methods_tools bodies are rebound onto server.py's
 * globals at install time (bind_module), so _ok/_err/_sessions/... are bare
 * free variables. _ok and _err are therefore injected seams ("ok"/"err");
 * _err is already claimed by src/tui_gateway/port_methods_tools.c and is
 * seamed, never re-ported. _plugin_rows / _ensure_plugin_activation_listener
 * / _portable_server_names are ported siblings here: flows seam them (each is
 * exercised by its own cases); the pure leaves (_joined_output,
 * _is_snapshot_restore, _worker_refused_skill, _refusal_data,
 * _with_activation) run REAL on both sides (inline transcriptions below).
 *
 * Seam name -> live call (args are informational projections):
 *   ok                          _ok(rid, result)
 *   err                         _err(rid, code, msg, data=None)  [data omitted when None]
 *   tools_mod                   _tools_mod(module) -> module marker string
 *   get_plugin_manager          hermes_cli.plugins.get_plugin_manager()
 *                               (in-function import; the import itself is silent)
 *   on_plugin_loaded            manager.on_plugin_loaded(_on_loaded)
 *                               [callback projects to the marker "_on_loaded"]
 *   ensure_plugin_activation_listener   _ensure_plugin_activation_listener()
 *   plugin_rows                 _plugin_rows() -> list of row dicts
 *   portable_server_names       _portable_server_names(Path(dir), key) -> set
 *                               [set projects to a SORTED list]
 *   config.load_config          hermes_cli.config.load_config()
 *   plugins_cmd.dashboard_set_agent_plugin_enabled(ident, enabled=...)
 *   plugins_cmd.dashboard_install_plugin(ident, force=, enable=, catalog_name=,
 *                               ref=, inspected_sha=)
 *   plugins_cmd.dashboard_update_user_plugin(name, accept_capabilities=, force=)
 *   plugins_cmd.dashboard_remove_user_plugin(name)
 *   plugins_cmd._find_plugin_entry(key) -> 6-tuple (list) | None
 *   plugins_cmd._is_portable_plugin_dir(plugin_dir)
 *   plugins_cmd_inspect.inspect_plugin_source(ident, catalog_name=...)
 *   desktop_plugin_install.install_desktop_plugin(ident, ref=, catalog_name=, force=)
 *   desktop_plugin_install.remove_desktop_plugin(name, force=)
 *   plugins_settings.save_plugin_settings(canonical, Path(dir)|None, values)
 *                               [Path projects to as_posix(); POSIX fixture paths]
 *   plugin_events.broadcast_plugin_event(canonical, "settings.changed", {...})
 *   plugins_auto_update.auto_update_state() / set_auto_update(bool)
 *   plugin_catalog_presence.onboarding_entries() / presence_for(names)
 *   agent_plugins.load_agent_plugin(plugin_dir, root)  [same path twice]
 *   plugins_manifest.portable_mcp_server_name(key, server)
 *   mcp_tool_discovery          _tools_mod("tools.mcp_tool_discovery") marker
 *   spawn_context_thread        agent.memory_provider.spawn_context_thread(fn,
 *                               args=(servers,), name=...)  [in-function import;
 *                               fn projects to its name; the servers SET projects
 *                               to a sorted list]
 *   thread.start                the spawned thread double's .start()
 *   <action handler>            _run_action's table[action](rid, params, *extra)
 *                               [table maps action -> seam name, fixture-declared]
 *
 * _PLUGIN_ID_RE (hermes_cli/plugin_events.py, ^[a-z0-9_-]{1,64}$) is a PURE
 * leaf: transcribed below (tmt_plugin_id_match), run real in the oracle; the
 * $-before-trailing-newline semantics are preserved. Not traced.
 *
 * Representation contract (shared with tests/oracle_tui_methods_tools_flow.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)}; BaseException-only
 *   kinds (SystemExit, KeyboardInterrupt, GeneratorExit, CancelledError) PASS
 *   THROUGH every except-Exception site.
 * - Scripted raises of plain kinds carry str(exc) == message verbatim (the
 *   oracle materializes real exception instances; KeyError/errno shapes are
 *   outside the fixture domain except the C-emitted KeyError specs, whose
 *   message is repr(key) exactly as str(KeyError(key))).
 * - Module doubles: tools_mod scripts return the module NAME string; the
 *   oracle resolves it to a SimpleNamespace of recording stubs. The
 *   PluginOperationError class is shared between make_exc and the module
 *   double so the except-clause identity matches.
 * - Manager/package/thread doubles: scripted dicts become SimpleNamespaces;
 *   .start()/.on_plugin_loaded record their own seams.
 * - Tuples project to lists; sets to sorted lists; integral floats to ints;
 *   Paths to POSIX strings (fixture paths are POSIX, no drive letters/UNC).
 * - str()/strip()/lower()/split() fold the ASCII fixture corpus only;
 *   params identifier/name/key values are strings or null (a truthy non-string
 *   raises AttributeError exactly as live .strip() does).
 * - State: ctx.state mirrors the module globals _plugin_activations (dict)
 *   and _plugin_activation_subscribed (set, kept sorted). Functions that touch
 *   them materialize BOTH keys at entry; other functions leave state alone.
 * - in.invoke_loaded (listener cases): after a successful registration the
 *   harness drives the registered _on_loaded callback with each summaries
 *   list, mutating _plugin_activations exactly as the live closure does.
 *
 * Ownership: tmt_call ADOPTS args into the trace event; its return is a fresh
 * owned node. json_set ADOPTS its value and frees the replaced node — borrowed
 * pointers to a replaced key are copied first. Seam results are caller-freed.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── universal seam context ── */
typedef struct tmt_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} tmt_ctx_t;

/* ── basic helpers ── */
static bool tmt_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static const char *tmt_raise_kind(const json_t *v) {
    return tmt_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}

/* BaseException-only kinds pass through every except-Exception site. */
static bool tmt_is_base_exc(const char *kind) {
    return kind && (!strcmp(kind, "SystemExit") || !strcmp(kind, "KeyboardInterrupt")
                    || !strcmp(kind, "GeneratorExit") || !strcmp(kind, "CancelledError"));
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool tmt_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static const char *tmt_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static json_t *tmt_err_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* Python type name for a JSON value (fixture scalar/container domain). */
static const char *tmt_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            return (d == (double)(long long)d) ? "int" : "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* Python str() for scalars (containers are outside the str()-projection
 * fixture domain; they serialize as JSON, which no fixture exercises). */
static char *tmt_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL: return strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && d > -9.2e18 && d < 9.2e18)
                snprintf(buf, sizeof buf, "%lld", (long long)d);
            else
                snprintf(buf, sizeof buf, "%.17g", d);
            return strdup(buf);
        }
        default: return json_serialize(v);
    }
}

static char *tmt_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

static bool tmt_is_ascii_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* str.strip(): ASCII whitespace both ends (fixtures fold ASCII only). */
static char *tmt_strip(const char *s) {
    if (!s) s = "";
    while (*s && tmt_is_ascii_ws((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && tmt_is_ascii_ws((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - s);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

/* str.lower(): ASCII fold (fixture corpus folds ASCII only). */
static char *tmt_lower(const char *s) {
    char *o = tmt_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}

/* (params.get(a) or params.get(b) or "").strip() with the live AttributeError
 * surface for a truthy non-string. Returns a malloc'd string, or a raise spec
 * via *raise (caller frees the NULL return's spec). */
static char *tmt_strip_param(const json_t *params, const char *a, const char *b,
                             json_t **raise_out) {
    *raise_out = NULL;
    const json_t *v = json_obj_get(params, a);
    if (!tmt_truthy(v) && b) v = json_obj_get(params, b);
    if (!tmt_truthy(v)) return tmt_strdup("");
    const char *s = tmt_str(v);
    if (!s) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'", tmt_type_name(v));
        *raise_out = tmt_err_spec("AttributeError", msg);
        return NULL;
    }
    return tmt_strip(s);
}

/* str(params.get(k) or "").strip() — always a string result. */
static char *tmt_str_strip_param(const json_t *params, const char *k) {
    const json_t *v = json_obj_get(params, k);
    char *s = tmt_truthy(v) ? tmt_pystr(v) : tmt_strdup("");
    char *out = tmt_strip(s);
    free(s);
    return out;
}

/* Deep scalar equality (Python == on the fixture domain; bool==int cross). */
static bool tmt_json_eq(const json_t *a, const json_t *b) {
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return false;
    if (a->type == JSON_NUMBER && b->type == JSON_NUMBER) return a->num_val == b->num_val;
    if (a->type == JSON_BOOL && b->type == JSON_BOOL) return a->bool_val == b->bool_val;
    if (a->type == JSON_BOOL && b->type == JSON_NUMBER)
        return (a->bool_val ? 1.0 : 0.0) == b->num_val;
    if (a->type == JSON_NUMBER && b->type == JSON_BOOL)
        return a->num_val == (b->bool_val ? 1.0 : 0.0);
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type != b->type) return false;
    if (a->type == JSON_ARRAY) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++)
            if (!tmt_json_eq(a->c.items[i], b->c.items[i])) return false;
        return true;
    }
    if (a->type == JSON_OBJECT) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !tmt_json_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}

/* _PLUGIN_ID_RE = re.compile(r"^[a-z0-9_-]{1,64}$") — re.match semantics:
 * $ also matches just before ONE trailing newline. Pure leaf, not traced. */
static bool tmt_plugin_id_match(const char *s) {
    if (!s) return false;
    size_t n = strlen(s);
    size_t eff = (n > 0 && s[n - 1] == '\n') ? n - 1 : n;
    if (eff < 1 || eff > 64) return false;
    for (size_t i = 0; i < eff; i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return false;
    }
    return true;
}

/* ── the one universal seam (fail-closed) ── */
static json_t *tmt_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return tmt_err_spec(type, msg);
}

static json_t *tmt_call(tmt_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    /* drop null-valued arg keys on both sides (absent-vs-null parity) */
    json_t *clean = json_object();
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++) {
            const json_t *v = args->c.items[i];
            if (v && v->type == JSON_NULL) continue;
            json_set(clean, args->c.keys[i], json_copy(v));
        }
    }
    json_free(args);
    json_set(ev, "args", clean);
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = tmt_err_spec("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = tmt_err_spec("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = tmt_make_exc(raise);
            } else {
                json_t *r = json_obj_get(used, "ret");
                ret = r ? json_copy(r) : json_null();
            }
        } else {
            ret = used ? json_copy(used) : json_null();
        }
    }
    json_set(ev, "ret", json_copy(ret));
    json_append(ctx->trace, ev);
    return ret;
}

/* args builders (values borrowed; copied into the args object) */
static json_t *tmt_args0(void) { return json_object(); }
static void tmt_aset(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static void tmt_asets(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void tmt_asetb(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void tmt_asetn(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }

/* _tools_mod(module): deferred import seam; returns the module marker. */
static json_t *tmt_tools_mod(tmt_ctx_t *ctx, const char *module) {
    json_t *a = tmt_args0();
    tmt_asets(a, "module", module);
    return tmt_call(ctx, "tools_mod", a);
}

/* _ok(rid, result) — result is ADOPTED. */
static json_t *tmt_ok(tmt_ctx_t *ctx, const json_t *rid, json_t *result) {
    json_t *a = tmt_args0();
    tmt_aset(a, "rid", rid);
    json_set(a, "result", result ? result : json_null());
    return tmt_call(ctx, "ok", a);
}

/* _err(rid, code, msg, data) — data ADOPTED (NULL omits the key). */
static json_t *tmt_err(tmt_ctx_t *ctx, const json_t *rid, int code,
                       const char *msg, json_t *data) {
    json_t *a = tmt_args0();
    tmt_aset(a, "rid", rid);
    tmt_asetn(a, "code", (double)code);
    tmt_asets(a, "message", msg);
    if (data) json_set(a, "data", data);
    return tmt_call(ctx, "err", a);
}

/* ── module-global state mirror ── */
static void tmt_ensure_globals(tmt_ctx_t *ctx) {
    if (!json_obj_get(ctx->state, "_plugin_activations"))
        json_set(ctx->state, "_plugin_activations", json_object());
    if (!json_obj_get(ctx->state, "_plugin_activation_subscribed"))
        json_set(ctx->state, "_plugin_activation_subscribed", json_array());
}

static json_t *tmt_activations(tmt_ctx_t *ctx) {
    return json_obj_get(ctx->state, "_plugin_activations");
}

/* set.add on the subscribed array; the array is kept SORTED (the live set
 * iterates arbitrarily, the oracle sorts — both sides emit sorted order). */
static int tmt_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void tmt_subscribed_add(tmt_ctx_t *ctx, const char *key) {
    json_t *sub = json_obj_get(ctx->state, "_plugin_activation_subscribed");
    if (!sub || sub->type != JSON_ARRAY) return;
    size_t n = json_len(sub);
    for (size_t i = 0; i < n; i++) {
        const char *s = tmt_str(json_get(sub, i));
        if (s && strcmp(s, key) == 0) return;
    }
    const char **vals = malloc((n + 1) * sizeof(char *));
    if (!vals) return;
    for (size_t i = 0; i < n; i++) vals[i] = tmt_str(json_get(sub, i));
    vals[n] = key;
    qsort(vals, n + 1, sizeof(char *), tmt_cmp_str);
    json_t *fresh = json_array();
    for (size_t i = 0; i < n + 1; i++)
        json_append(fresh, json_string(vals[i] ? vals[i] : ""));
    free(vals);
    json_set(ctx->state, "_plugin_activation_subscribed", fresh);
}
/* ── pure leaves (run REAL on both sides; no seams) ── */

/* PoP: tmt_joined_output @ tui_gateway/methods_tools.py:_joined_output */
static json_t *tmt_joined_output(tmt_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *r = json_obj_get(args, "r");
    if (!r || r->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'stdout'", tmt_type_name(r));
        return tmt_err_spec("AttributeError", msg);
    }
    const json_t *so = json_obj_get(r, "stdout");
    const json_t *se = json_obj_get(r, "stderr");
    const char *a = tmt_truthy(so) ? tmt_str(so) : NULL;
    const char *b = tmt_truthy(se) ? tmt_str(se) : NULL;
    /* "\n".join(p for p in (stdout or "", stderr or "") if p).strip() */
    size_t la = a ? strlen(a) : 0, lb = b ? strlen(b) : 0;
    size_t cap = la + lb + 2;
    char *joined = malloc(cap);
    if (!joined) return tmt_err_spec("MemoryError", "");
    size_t k = 0;
    if (a) { memcpy(joined + k, a, la); k += la; }
    if (a && b) joined[k++] = '\n';
    if (b) { memcpy(joined + k, b, lb); k += lb; }
    joined[k] = '\0';
    char *out = tmt_strip(joined);
    free(joined);
    json_t *ret = json_string(out ? out : "");
    free(out);
    return ret;
}

/* (arg.split(maxsplit=1)[0].lower() if arg else "") in {"restore","rewind"} */
static bool tmt_snapshot_restore_v(const json_t *arg, json_t **raise_out) {
    *raise_out = NULL;
    if (!tmt_truthy(arg)) return false;
    const char *s = tmt_str(arg);
    if (!s) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'split'", tmt_type_name(arg));
        *raise_out = tmt_err_spec("AttributeError", msg);
        return false;
    }
    while (*s && tmt_is_ascii_ws((unsigned char)*s)) s++;
    if (!*s) {
        /* "   ".split(maxsplit=1) == [] -> [0] IndexError */
        *raise_out = tmt_err_spec("IndexError", "list index out of range");
        return false;
    }
    const char *end = s;
    while (*end && !tmt_is_ascii_ws((unsigned char)*end)) end++;
    size_t n = (size_t)(end - s);
    char *tok = malloc(n + 1);
    if (!tok) return false;
    memcpy(tok, s, n);
    tok[n] = '\0';
    char *low = tmt_lower(tok);
    free(tok);
    bool hit = low && (!strcmp(low, "restore") || !strcmp(low, "rewind"));
    free(low);
    return hit;
}

/* PoP: tmt_is_snapshot_restore @ tui_gateway/methods_tools.py:_is_snapshot_restore */
static json_t *tmt_is_snapshot_restore(tmt_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *raise = NULL;
    bool hit = tmt_snapshot_restore_v(json_obj_get(args, "arg"), &raise);
    if (raise) return raise;
    return json_bool(hit);
}

/* PoP: tmt_worker_refused_skill @ tui_gateway/methods_tools.py:_worker_refused_skill */
static json_t *tmt_worker_refused_skill(tmt_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    /* _SKILL_WORKER_REFUSED in str(exc); the exc double is a {type,message}
     * spec materialized as a real exception with str(exc) == message. */
    const json_t *exc = json_obj_get(args, "exc");
    const char *msg = exc ? json_get_str(exc, "message", "") : "";
    return json_bool(strstr(msg, "skill command refused before process: /") != NULL);
}

/* _refusal_data core on a borrowed result dict (caller guarantees object). */
static json_t *tmt_refusal_data_v(const json_t *result) {
    const char *reason = NULL;
    if (tmt_truthy(json_obj_get(result, "source_moved"))) reason = "source_moved";
    else if (tmt_truthy(json_obj_get(result, "exists"))) reason = "exists";
    if (!reason) return json_null();
    json_t *out = json_object();
    json_set_hdr(out, "reason", reason);
    return out;
}

/* PoP: tmt_refusal_data @ tui_gateway/methods_tools.py:_refusal_data */
static json_t *tmt_refusal_data(tmt_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *result = json_obj_get(args, "result");
    if (!result || result->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", tmt_type_name(result));
        return tmt_err_spec("AttributeError", msg);
    }
    return tmt_refusal_data_v(result);
}

/* _with_activation(result, name) — real leaf; mutates result in place and
 * reads the _plugin_activations module global (ctx.state). */
static json_t *tmt_with_activation_v(tmt_ctx_t *ctx, json_t *result, const json_t *name) {
    tmt_ensure_globals(ctx);
    if (!result || result->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", tmt_type_name(result));
        json_free(result);
        return tmt_err_spec("AttributeError", msg);
    }
    if (tmt_truthy(json_obj_get(result, "activation"))) return result;
    const json_t *acts = tmt_activations(ctx);
    const json_t *keys[3] = {
        name, json_obj_get(result, "plugin_name"), json_obj_get(result, "name"),
    };
    for (int i = 0; i < 3; i++) {
        const char *k = tmt_str(keys[i]);
        if (!tmt_truthy(keys[i]) || !k) continue;
        const json_t *entry = json_obj_get(acts, k);
        if (entry) {
            json_set(result, "activation", json_copy(entry));
            break;
        }
    }
    return result;
}

/* PoP: tmt_with_activation @ tui_gateway/methods_tools.py:_with_activation */
static json_t *tmt_with_activation(tmt_ctx_t *ctx, const json_t *args) {
    json_t *result = json_copy(json_obj_get(args, "result"));
    return tmt_with_activation_v(ctx, result, json_obj_get(args, "name"));
}

/* ── shared flows ── */

/* PoP: tmt_exec_out @ tui_gateway/methods_tools.py:_exec_out */
static json_t *tmt_exec_out(tmt_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    json_t *payload = json_object();
    json_set_hdr(payload, "type", "exec");
    tmt_aset(payload, "output", json_obj_get(args, "output"));
    return tmt_ok(ctx, rid, payload);
}

/* PoP: tmt_run_action @ tui_gateway/methods_tools.py:_run_action */
static json_t *tmt_run_action(tmt_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    const json_t *table = json_obj_get(args, "table");
    const json_t *extra = json_obj_get(args, "extra");
    if (!params || params->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", tmt_type_name(params));
        return tmt_err_spec("AttributeError", msg);
    }
    const json_t *action = json_obj_get(params, "action");
    json_t *action_owned = action ? json_copy(action) : json_string("list");
    const char *action_s = tmt_str(action_owned);
    const json_t *handler = (table && table->type == JSON_OBJECT && action_s)
        ? json_obj_get(table, action_s) : NULL;
    if (!handler) {
        char *as = tmt_pystr(action_owned);
        char *ls = tmt_pystr(json_obj_get(args, "label"));
        size_t cap = strlen(as) + strlen(ls) + 64;
        char *msg = malloc(cap);
        if (msg) snprintf(msg, cap, "unknown %s action: %s", ls, as);
        free(as);
        free(ls);
        json_free(action_owned);
        json_t *e = tmt_err(ctx, rid, 4017, msg ? msg : "", NULL);
        free(msg);
        return e;
    }
    const char *seam = tmt_str(handler);
    if (!seam) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not callable", tmt_type_name(handler));
        json_free(action_owned);
        return tmt_err_spec("TypeError", msg);
    }
    json_t *a = tmt_args0();
    tmt_aset(a, "rid", rid);
    tmt_aset(a, "params", params);
    json_set(a, "extra", (extra && extra->type == JSON_ARRAY) ? json_copy(extra) : json_array());
    json_free(action_owned);
    return tmt_call(ctx, seam, a);
}

/* PoP: tmt_cmd_queue @ tui_gateway/methods_tools.py:_cmd_queue */
static json_t *tmt_cmd_queue(tmt_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *arg = json_obj_get(args, "arg");
    if (tmt_truthy(arg)) {
        json_t *payload = json_object();
        json_set_hdr(payload, "type", "send");
        tmt_aset(payload, "message", arg);
        return tmt_ok(ctx, rid, payload);
    }
    return tmt_err(ctx, rid, 4004, "usage: /queue <prompt>", NULL);
}

/* PoP: tmt_cmd_snapshot @ tui_gateway/methods_tools.py:_cmd_snapshot */
static json_t *tmt_cmd_snapshot(tmt_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    json_t *raise = NULL;
    bool hit = tmt_snapshot_restore_v(json_obj_get(args, "arg"), &raise);
    if (raise) return raise;
    if (!hit) return json_null();
    json_t *payload = json_object();
    json_set_hdr(payload, "type", "exec");
    json_set_hdr(payload, "output",
                 "/snapshot restore is blocked in the TUI because it changes config/state on disk "
                 "while the live agent has cached settings. Run it in the classic CLI, then restart the TUI.");
    return tmt_ok(ctx, rid, payload);
}

/* PoP: tmt_mcp_reload_confirm_required @ tui_gateway/methods_tools.py:_mcp_reload_confirm_required */
static json_t *tmt_mcp_reload_confirm_required(tmt_ctx_t *ctx, const json_t *args) {
    (void)args;
    /* try: cfg = _tools_mod("hermes_cli.config").load_config()
     *      approvals = cfg.get("approvals") if isinstance(cfg, dict) else None
     *      return bool(approvals.get("mcp_reload_confirm", True))
     *             if isinstance(approvals, dict) else True
     * except Exception: return True   (BaseException passes through) */
    json_t *m = tmt_tools_mod(ctx, "hermes_cli.config");
    if (tmt_is_raise(m)) {
        if (tmt_is_base_exc(tmt_raise_kind(m))) return m;
        json_free(m);
        return json_bool(true);
    }
    json_free(m);
    json_t *cfg = tmt_call(ctx, "config.load_config", tmt_args0());
    if (tmt_is_raise(cfg)) {
        if (tmt_is_base_exc(tmt_raise_kind(cfg))) return cfg;
        json_free(cfg);
        return json_bool(true);
    }
    bool out = true;
    if (cfg && cfg->type == JSON_OBJECT) {
        const json_t *approvals = json_obj_get(cfg, "approvals");
        if (approvals && approvals->type == JSON_OBJECT) {
            const json_t *v = json_obj_get(approvals, "mcp_reload_confirm");
            out = v ? tmt_truthy(v) : true; /* .get default True only when ABSENT */
        }
    }
    json_free(cfg);
    return json_bool(out);
}

/* PoP: tmt_session_key_or_err @ tui_gateway/methods_tools.py:_session_key_or_err */
static json_t *tmt_session_key_or_err(tmt_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *session = json_obj_get(args, "session");
    const char *module = json_get_str(args, "module", "");
    const char *label = json_get_str(args, "label", "");
    json_t *none3 = json_array();
    json_append(none3, json_null());
    json_append(none3, json_null());
    if (!tmt_truthy(session)) {
        json_append(none3, tmt_err(ctx, rid, 4001, "no active session", NULL));
        return none3;
    }
    if (session->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", tmt_type_name(session));
        json_free(none3);
        return tmt_err_spec("AttributeError", msg);
    }
    const json_t *sid_key = json_obj_get(session, "session_key");
    if (!tmt_truthy(sid_key)) {
        json_append(none3, tmt_err(ctx, rid, 4001, "no session key", NULL));
        return none3;
    }
    json_t *m = tmt_tools_mod(ctx, module);
    if (tmt_is_raise(m)) {
        if (tmt_is_base_exc(tmt_raise_kind(m))) { json_free(none3); return m; }
        size_t cap = strlen(label) + strlen(json_get_str(m, "message", "")) + 32;
        char *msg = malloc(cap);
        if (msg) snprintf(msg, cap, "%s unavailable: %s", label, json_get_str(m, "message", ""));
        json_free(m);
        json_append(none3, tmt_err(ctx, rid, 5030, msg ? msg : "", NULL));
        free(msg);
        return none3;
    }
    json_t *ret = json_array();
    json_append(ret, json_copy(sid_key));
    json_append(ret, m);
    json_append(ret, json_null());
    json_free(none3);
    return ret;
}

/* PoP: tmt_ensure_plugin_activation_listener @ tui_gateway/methods_tools.py:_ensure_plugin_activation_listener */
static json_t *tmt_ensure_plugin_activation_listener(tmt_ctx_t *ctx, const json_t *args) {
    (void)args;
    tmt_ensure_globals(ctx);
    /* from hermes_cli.plugins import get_plugin_manager (silent import) */
    json_t *m = tmt_call(ctx, "get_plugin_manager", tmt_args0());
    if (tmt_is_raise(m)) return m;
    if (!m || m->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'scope_key'", tmt_type_name(m));
        json_free(m);
        return tmt_err_spec("AttributeError", msg);
    }
    const json_t *sk = json_obj_get(m, "scope_key");
    if (!sk) {
        json_free(m);
        return tmt_err_spec("AttributeError",
                            "'types.SimpleNamespace' object has no attribute 'scope_key'");
    }
    char *key = tmt_pystr(sk);
    json_free(m);
    json_t *sub = json_obj_get(ctx->state, "_plugin_activation_subscribed");
    size_t n = json_len(sub);
    for (size_t i = 0; i < n; i++) {
        const char *s = tmt_str(json_get(sub, i));
        if (s && strcmp(s, key) == 0) { free(key); return json_null(); }
    }
    tmt_subscribed_add(ctx, key);
    free(key);
    json_t *a = tmt_args0();
    tmt_asets(a, "callback", "_on_loaded");
    json_t *r = tmt_call(ctx, "on_plugin_loaded", a);
    if (tmt_is_raise(r)) return r;
    json_free(r);
    return json_null();
}
/* ── plugins-hub flows ── */

/* Seam result that live reads with .get(): non-dict -> AttributeError.
 * A raise spec passes through untouched. Returns the same owned node. */
static json_t *tmt_need_obj(json_t *r, const char *attr) {
    if (tmt_is_raise(r)) return r;
    if (!r || r->type != JSON_OBJECT) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", tmt_type_name(r), attr);
        json_free(r);
        return tmt_err_spec("AttributeError", msg);
    }
    return r;
}

/* rows[i][key] with the live KeyError/TypeError surface. Borrowed return. */
static const json_t *tmt_row_get(const json_t *row, const char *key, json_t **raise_out) {
    *raise_out = NULL;
    if (!row || row->type != JSON_OBJECT) {
        char msg[128];
        if (row && row->type == JSON_STRING)
            snprintf(msg, sizeof msg, "string indices must be integers, not 'str'");
        else
            snprintf(msg, sizeof msg, "'%s' object is not subscriptable", tmt_type_name(row));
        *raise_out = tmt_err_spec("TypeError", msg);
        return NULL;
    }
    const json_t *v = json_obj_get(row, key);
    if (!v) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s'", key); /* str(KeyError(k)) == repr(k) */
        *raise_out = tmt_err_spec("KeyError", msg);
    }
    return v;
}

/* result.get("error") or <default> — fixture error values are strings/null. */
static const char *tmt_error_or(const json_t *result, const char *def) {
    const json_t *e = json_obj_get(result, "error");
    if (tmt_truthy(e)) {
        const char *s = tmt_str(e);
        if (s) return s;
    }
    return def;
}

/* params guard for flows that read params.get(...). */
static json_t *tmt_params_guard(const json_t *params) {
    if (!params || params->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", tmt_type_name(params));
        return tmt_err_spec("AttributeError", msg);
    }
    return NULL;
}

/* PoP: tmt_plugins_list @ tui_gateway/methods_tools.py:_plugins_list */
static json_t *tmt_plugins_list(tmt_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    json_t *rows = tmt_call(ctx, "plugin_rows", tmt_args0());
    if (tmt_is_raise(rows)) return rows;
    if (!rows || rows->type != JSON_ARRAY) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", tmt_type_name(rows));
        json_free(rows);
        return tmt_err_spec("TypeError", msg);
    }
    size_t n = json_len(rows);
    size_t user_count = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *raise = NULL;
        const json_t *src = tmt_row_get(json_get(rows, i), "source", &raise);
        if (raise) { json_free(rows); return raise; }
        const char *s = tmt_str(src);
        if (!s || strcmp(s, "bundled") != 0) user_count++;
    }
    json_t *payload = json_object();
    json_set(payload, "plugins", rows);
    json_set(payload, "user_count", json_number((double)user_count));
    json_set(payload, "bundled_count", json_number((double)(n - user_count)));
    return tmt_ok(ctx, rid, payload);
}

/* PoP: tmt_plugins_toggle @ tui_gateway/methods_tools.py:_plugins_toggle */
static json_t *tmt_plugins_toggle(tmt_ctx_t *ctx, const json_t *args) {
    tmt_ensure_globals(ctx); /* module globals exist for the whole call */
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    json_t *g = tmt_params_guard(params);
    if (g) return g;
    json_t *raise = NULL;
    char *ident = tmt_strip_param(params, "key", "name", &raise);
    if (raise) return raise;
    if (!ident[0]) {
        free(ident);
        return tmt_err(ctx, rid, 4019, "plugins.toggle requires a 'key' or 'name'", NULL);
    }
    json_t *r = tmt_call(ctx, "ensure_plugin_activation_listener", tmt_args0());
    if (tmt_is_raise(r)) { free(ident); return r; }
    json_free(r);
    json_t *m = tmt_tools_mod(ctx, "hermes_cli.plugins_cmd");
    if (tmt_is_raise(m)) { free(ident); return m; }
    json_free(m);
    json_t *a = tmt_args0();
    tmt_asets(a, "ident", ident);
    tmt_asetb(a, "enabled", tmt_truthy(json_obj_get(params, "enable")));
    json_t *result = tmt_call(ctx, "plugins_cmd.dashboard_set_agent_plugin_enabled", a);
    result = tmt_need_obj(result, "get");
    if (tmt_is_raise(result)) { free(ident); return result; }
    if (!tmt_truthy(json_obj_get(result, "ok"))) {
        json_t *e = tmt_err(ctx, rid, 5026, tmt_error_or(result, "toggle failed"), NULL);
        json_free(result);
        free(ident);
        return e;
    }
    const json_t *rn = json_obj_get(result, "name");
    json_t *key = tmt_truthy(rn) ? json_copy(rn) : json_string(ident);
    free(ident);
    json_t *rows = tmt_call(ctx, "plugin_rows", tmt_args0());
    if (tmt_is_raise(rows)) { json_free(key); json_free(result); return rows; }
    if (!rows || rows->type != JSON_ARRAY) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", tmt_type_name(rows));
        json_free(rows);
        json_free(key);
        json_free(result);
        return tmt_err_spec("TypeError", msg);
    }
    const json_t *row = NULL;
    for (size_t i = 0; i < json_len(rows); i++) {
        const json_t *rrow = json_get(rows, i);
        json_t *rk_raise = NULL;
        const json_t *rk = tmt_row_get(rrow, "key", &rk_raise);
        if (rk_raise) { json_free(rows); json_free(key); json_free(result); return rk_raise; }
        const json_t *rname = tmt_row_get(rrow, "name", &rk_raise);
        if (rk_raise) { json_free(rows); json_free(key); json_free(result); return rk_raise; }
        if (tmt_json_eq(key, rk) || tmt_json_eq(key, rname)) { row = rrow; break; }
    }
    json_t *payload = json_object();
    tmt_asetb(payload, "ok", true);
    tmt_asetb(payload, "unchanged", tmt_truthy(json_obj_get(result, "unchanged")));
    tmt_asetb(payload, "restart_required", tmt_truthy(json_obj_get(result, "restart_required")));
    tmt_asetb(payload, "gateway_reloaded", tmt_truthy(json_obj_get(result, "gateway_reloaded")));
    tmt_aset(payload, "activation", json_obj_get(result, "activation"));
    json_set(payload, "name", json_copy(key));
    json_set(payload, "plugin", row ? json_copy(row) : json_null());
    json_free(rows);
    json_free(result);
    payload = tmt_with_activation_v(ctx, payload, key);
    json_free(key);
    if (tmt_is_raise(payload)) return payload;
    return tmt_ok(ctx, rid, payload);
}

/* PoP: tmt_plugins_install @ tui_gateway/methods_tools.py:_plugins_install */
static json_t *tmt_plugins_install(tmt_ctx_t *ctx, const json_t *args) {
    tmt_ensure_globals(ctx); /* module globals exist for the whole call */
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    json_t *g = tmt_params_guard(params);
    if (g) return g;
    json_t *raise = NULL;
    char *ident = tmt_strip_param(params, "identifier", "repo", &raise);
    if (raise) return raise;
    char *catalog_name = tmt_str_strip_param(params, "catalog_name");
    if (!ident[0] && !catalog_name[0]) {
        free(ident);
        free(catalog_name);
        return tmt_err(ctx, rid, 4019,
                       "plugins.install requires 'identifier', 'repo', or 'catalog_name'", NULL);
    }
    json_t *r = tmt_call(ctx, "ensure_plugin_activation_listener", tmt_args0());
    if (tmt_is_raise(r)) { free(ident); free(catalog_name); return r; }
    json_free(r);
    json_t *m = tmt_tools_mod(ctx, "hermes_cli.plugins_cmd");
    if (tmt_is_raise(m)) { free(ident); free(catalog_name); return m; }
    json_free(m);
    char *ref = tmt_str_strip_param(params, "ref");
    char *isha = tmt_str_strip_param(params, "inspected_sha");
    json_t *a = tmt_args0();
    tmt_asets(a, "ident", ident);
    tmt_asetb(a, "force", tmt_truthy(json_obj_get(params, "force")));
    const json_t *enable = json_obj_get(params, "enable");
    json_set(a, "enable", enable ? json_copy(enable) : json_bool(true));
    tmt_asets(a, "catalog_name", catalog_name[0] ? catalog_name : NULL);
    tmt_asets(a, "ref", ref[0] ? ref : NULL);
    tmt_asets(a, "inspected_sha", isha[0] ? isha : NULL);
    free(ident);
    free(catalog_name);
    free(ref);
    free(isha);
    json_t *result = tmt_call(ctx, "plugins_cmd.dashboard_install_plugin", a);
    result = tmt_need_obj(result, "get");
    if (tmt_is_raise(result)) return result;
    if (!tmt_truthy(json_obj_get(result, "ok"))) {
        json_t *e = tmt_err(ctx, rid, 5026, tmt_error_or(result, "install failed"),
                            tmt_refusal_data_v(result));
        json_free(result);
        return e;
    }
    json_t *payload;
    if (tmt_truthy(json_obj_get(result, "enabled"))) {
        const json_t *pn = json_obj_get(result, "plugin_name");
        char *pns = tmt_truthy(pn) ? tmt_pystr(pn) : tmt_strdup("");
        json_t *name_node = json_string(pns);
        free(pns);
        payload = tmt_with_activation_v(ctx, result, name_node);
        json_free(name_node);
        if (tmt_is_raise(payload)) return payload;
    } else {
        payload = result;
    }
    return tmt_ok(ctx, rid, payload);
}

/* PoP: tmt_plugins_inspect @ tui_gateway/methods_tools.py:_plugins_inspect */
static json_t *tmt_plugins_inspect(tmt_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    json_t *g = tmt_params_guard(params);
    if (g) return g;
    json_t *raise = NULL;
    char *ident = tmt_strip_param(params, "identifier", "repo", &raise);
    if (raise) return raise;
    char *catalog_name = tmt_str_strip_param(params, "catalog_name");
    if (!ident[0] && !catalog_name[0]) {
        free(ident);
        free(catalog_name);
        return tmt_err(ctx, rid, 4019,
                       "plugins.inspect requires 'identifier', 'repo', or 'catalog_name'", NULL);
    }
    json_t *m = tmt_tools_mod(ctx, "hermes_cli.plugins_cmd_inspect");
    if (tmt_is_raise(m)) { free(ident); free(catalog_name); return m; }
    json_free(m);
    json_t *a = tmt_args0();
    tmt_asets(a, "ident", ident);
    tmt_asets(a, "catalog_name", catalog_name[0] ? catalog_name : NULL);
    free(ident);
    free(catalog_name);
    json_t *result = tmt_call(ctx, "plugins_cmd_inspect.inspect_plugin_source", a);
    if (tmt_is_raise(result)) return result;
    return tmt_ok(ctx, rid, result);
}

/* PoP: tmt_plugins_install_desktop @ tui_gateway/methods_tools.py:_plugins_install_desktop */
static json_t *tmt_plugins_install_desktop(tmt_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    json_t *g = tmt_params_guard(params);
    if (g) return g;
    json_t *raise = NULL;
    char *ident = tmt_strip_param(params, "identifier", "repo", &raise);
    if (raise) return raise;
    char *catalog_name = tmt_str_strip_param(params, "catalog_name");
    if (!ident[0] && !catalog_name[0]) {
        free(ident);
        free(catalog_name);
        return tmt_err(ctx, rid, 4019,
                       "plugins.manage install_desktop requires 'identifier', 'repo', or 'catalog_name'",
                       NULL);
    }
    json_t *m = tmt_tools_mod(ctx, "hermes_cli.desktop_plugin_install");
    if (tmt_is_raise(m)) { free(ident); free(catalog_name); return m; }
    json_free(m);
    json_t *a = tmt_args0();
    tmt_asets(a, "ident", ident);
    tmt_aset(a, "ref", json_obj_get(params, "ref")); /* raw, no str/strip */
    tmt_asets(a, "catalog_name", catalog_name[0] ? catalog_name : NULL);
    tmt_asetb(a, "force", tmt_truthy(json_obj_get(params, "force")));
    free(ident);
    free(catalog_name);
    json_t *result = tmt_call(ctx, "desktop_plugin_install.install_desktop_plugin", a);
    result = tmt_need_obj(result, "get");
    if (tmt_is_raise(result)) return result;
    if (!tmt_truthy(json_obj_get(result, "ok"))) {
        json_t *e = tmt_err(ctx, rid, 5026, tmt_error_or(result, "desktop install failed"),
                            tmt_refusal_data_v(result));
        json_free(result);
        return e;
    }
    return tmt_ok(ctx, rid, result);
}

/* PoP: tmt_plugins_remove_desktop @ tui_gateway/methods_tools.py:_plugins_remove_desktop */
static json_t *tmt_plugins_remove_desktop(tmt_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    json_t *g = tmt_params_guard(params);
    if (g) return g;
    json_t *raise = NULL;
    char *name = tmt_strip_param(params, "name", "key", &raise);
    if (raise) return raise;
    if (!name[0]) {
        free(name);
        return tmt_err(ctx, rid, 4019, "plugins.manage remove_desktop requires a 'name'", NULL);
    }
    json_t *m = tmt_tools_mod(ctx, "hermes_cli.desktop_plugin_install");
    if (tmt_is_raise(m)) { free(name); return m; }
    json_free(m);
    json_t *a = tmt_args0();
    tmt_asets(a, "name", name);
    tmt_asetb(a, "force", tmt_truthy(json_obj_get(params, "force")));
    free(name);
    json_t *result = tmt_call(ctx, "desktop_plugin_install.remove_desktop_plugin", a);
    result = tmt_need_obj(result, "get");
    if (tmt_is_raise(result)) return result;
    if (tmt_truthy(json_obj_get(result, "ok"))) return tmt_ok(ctx, rid, result);
    json_t *e = tmt_err(ctx, rid, 5026, tmt_error_or(result, "desktop remove failed"), NULL);
    json_free(result);
    return e;
}
/* PoP: tmt_plugins_update @ tui_gateway/methods_tools.py:_plugins_update */
static json_t *tmt_plugins_update(tmt_ctx_t *ctx, const json_t *args) {
    tmt_ensure_globals(ctx); /* module globals exist for the whole call */
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    json_t *g = tmt_params_guard(params);
    if (g) return g;
    json_t *raise = NULL;
    char *name = tmt_strip_param(params, "name", NULL, &raise);
    if (raise) return raise;
    if (!name[0]) {
        free(name);
        return tmt_err(ctx, rid, 4019, "plugins.update requires a 'name'", NULL);
    }
    json_t *r = tmt_call(ctx, "ensure_plugin_activation_listener", tmt_args0());
    if (tmt_is_raise(r)) { free(name); return r; }
    json_free(r);
    json_t *m = tmt_tools_mod(ctx, "hermes_cli.plugins_cmd");
    if (tmt_is_raise(m)) { free(name); return m; }
    json_free(m);
    json_t *a = tmt_args0();
    tmt_asets(a, "name", name);
    tmt_asetb(a, "accept_capabilities", tmt_truthy(json_obj_get(params, "accept_capabilities")));
    tmt_asetb(a, "force", tmt_truthy(json_obj_get(params, "force")));
    json_t *result = tmt_call(ctx, "plugins_cmd.dashboard_update_user_plugin", a);
    result = tmt_need_obj(result, "get");
    if (tmt_is_raise(result)) { free(name); return result; }
    if (tmt_truthy(json_obj_get(result, "consent_required"))
        || tmt_truthy(json_obj_get(result, "scan_blocked"))) {
        static const char *keys[] = {
            "ok", "name", "sha", "error", "consent_required", "delta", "delta_lines",
            "scan_blocked", "scan_verdict", "scan_findings",
        };
        json_t *payload = json_object();
        for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
            if (json_has(result, keys[i]))
                json_set(payload, keys[i], json_copy(json_obj_get(result, keys[i])));
        json_free(result);
        free(name);
        return tmt_ok(ctx, rid, payload);
    }
    if (!tmt_truthy(json_obj_get(result, "ok"))) {
        json_t *e = tmt_err(ctx, rid, 4021, tmt_error_or(result, "update failed"), NULL);
        json_free(result);
        free(name);
        return e;
    }
    json_t *payload = json_object();
    tmt_asetb(payload, "ok", true);
    tmt_asetb(payload, "unchanged", tmt_truthy(json_obj_get(result, "unchanged")));
    const json_t *rname = json_obj_get(result, "name");
    json_set(payload, "name", tmt_truthy(rname) ? json_copy(rname) : json_string(name));
    /* warnings: list(result.get("warnings") or []) */
    const json_t *w = json_obj_get(result, "warnings");
    json_t *warnings = json_array();
    if (tmt_truthy(w)) {
        if (w->type == JSON_ARRAY) {
            json_free(warnings);
            warnings = json_copy(w);
        } else if (w->type == JSON_STRING) {
            for (const char *p = w->str_val; p && *p; p++) {
                char one[2] = { *p, '\0' };
                json_append(warnings, json_string(one));
            }
        } else if (w->type == JSON_OBJECT) {
            for (size_t i = 0; i < w->c.count; i++)
                json_append(warnings, json_string(w->c.keys[i]));
        } else {
            char msg[128];
            snprintf(msg, sizeof msg, "'%s' object is not iterable", tmt_type_name(w));
            json_free(warnings);
            json_free(payload);
            json_free(result);
            free(name);
            return tmt_err_spec("TypeError", msg);
        }
    }
    json_set(payload, "warnings", warnings);
    static const char *opt_keys[] = { "sha", "gateway_reloaded", "activation", "restart_required" };
    for (size_t i = 0; i < sizeof(opt_keys) / sizeof(opt_keys[0]); i++)
        if (json_has(result, opt_keys[i]))
            json_set(payload, opt_keys[i], json_copy(json_obj_get(result, opt_keys[i])));
    bool unchanged = tmt_truthy(json_obj_get(result, "unchanged"));
    json_free(result);
    free(name);
    if (unchanged) return tmt_ok(ctx, rid, payload);
    payload = tmt_with_activation_v(ctx, payload, json_obj_get(payload, "name"));
    if (tmt_is_raise(payload)) return payload;
    return tmt_ok(ctx, rid, payload);
}

/* PoP: tmt_plugins_remove @ tui_gateway/methods_tools.py:_plugins_remove */
static json_t *tmt_plugins_remove(tmt_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    json_t *g = tmt_params_guard(params);
    if (g) return g;
    json_t *raise = NULL;
    char *name = tmt_strip_param(params, "name", NULL, &raise);
    if (raise) return raise;
    if (!name[0]) {
        free(name);
        return tmt_err(ctx, rid, 4019, "plugins.remove requires a 'name'", NULL);
    }
    json_t *m = tmt_tools_mod(ctx, "hermes_cli.plugins_cmd");
    if (tmt_is_raise(m)) { free(name); return m; }
    json_free(m);
    json_t *a = tmt_args0();
    tmt_asets(a, "name", name);
    free(name);
    json_t *result = tmt_call(ctx, "plugins_cmd.dashboard_remove_user_plugin", a);
    result = tmt_need_obj(result, "get");
    if (tmt_is_raise(result)) return result;
    if (tmt_truthy(json_obj_get(result, "ok"))) return tmt_ok(ctx, rid, result);
    json_t *e = tmt_err(ctx, rid, 5026, tmt_error_or(result, "remove failed"), NULL);
    json_free(result);
    return e;
}

/* sorted unique projection of a string array (Python set -> sorted list). */
static json_t *tmt_sorted_str_set(const json_t *arr) {
    size_t n = (arr && arr->type == JSON_ARRAY) ? json_len(arr) : 0;
    char **vals = malloc((n + 1) * sizeof(char *));
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *v = json_get(arr, i);
        char *s = tmt_pystr(v);
        if (!s) continue;
        bool dup = false;
        for (size_t j = 0; j < k; j++)
            if (!strcmp(vals[j], s)) { dup = true; break; }
        if (dup) free(s);
        else vals[k++] = s;
    }
    qsort(vals, k, sizeof(char *), tmt_cmp_str);
    json_t *out = json_array();
    for (size_t i = 0; i < k; i++) {
        json_append(out, json_string(vals[i]));
        free(vals[i]);
    }
    free(vals);
    return out;
}

/* PoP: tmt_portable_server_names @ tui_gateway/methods_tools.py:_portable_server_names */
static json_t *tmt_portable_server_names(tmt_ctx_t *ctx, const json_t *args) {
    const char *plugin_dir = json_get_str(args, "plugin_dir", "");
    const json_t *key = json_obj_get(args, "key");
    json_t *m = tmt_tools_mod(ctx, "hermes_cli.agent_plugins");
    if (tmt_is_raise(m)) return m;
    json_free(m);
    json_t *a = tmt_args0();
    tmt_asets(a, "plugin_dir", plugin_dir);
    tmt_asets(a, "root", plugin_dir); /* live passes the same Path twice */
    json_t *pkg = tmt_call(ctx, "agent_plugins.load_agent_plugin", a);
    if (tmt_is_raise(pkg)) return pkg;
    /* name_for = _tools_mod("hermes_cli.plugins_manifest")... runs BEFORE the
     * comprehension touches package.mcp_servers (live statement order). */
    json_t *m2 = tmt_tools_mod(ctx, "hermes_cli.plugins_manifest");
    if (tmt_is_raise(m2)) { json_free(pkg); return m2; }
    json_free(m2);
    if (!pkg || pkg->type != JSON_OBJECT) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'mcp_servers'", tmt_type_name(pkg));
        json_free(pkg);
        return tmt_err_spec("AttributeError", msg);
    }
    const json_t *servers = json_obj_get(pkg, "mcp_servers");
    if (!servers) {
        json_free(pkg);
        return tmt_err_spec("AttributeError",
                            "'types.SimpleNamespace' object has no attribute 'mcp_servers'");
    }
    if (servers->type != JSON_ARRAY) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", tmt_type_name(servers));
        json_free(pkg);
        return tmt_err_spec("TypeError", msg);
    }
    json_t *names = json_array();
    for (size_t i = 0; i < json_len(servers); i++) {
        json_t *na = tmt_args0();
        tmt_aset(na, "key", key);
        tmt_aset(na, "server", json_get(servers, i));
        json_t *nm = tmt_call(ctx, "plugins_manifest.portable_mcp_server_name", na);
        if (tmt_is_raise(nm)) { json_free(names); json_free(pkg); return nm; }
        json_append(names, nm);
    }
    json_free(pkg);
    json_t *out = tmt_sorted_str_set(names);
    json_free(names);
    return out;
}

/* PoP: tmt_plugins_settings @ tui_gateway/methods_tools.py:_plugins_settings */
static json_t *tmt_plugins_settings(tmt_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    json_t *g = tmt_params_guard(params);
    if (g) return g;
    json_t *raise = NULL;
    char *key = tmt_strip_param(params, "key", "name", &raise);
    if (raise) return raise;
    const json_t *values = json_obj_get(params, "values");
    if (!key[0] || !values || values->type != JSON_OBJECT) {
        free(key);
        return tmt_err(ctx, rid, 4019,
                       "plugins.settings requires a 'key' and a 'values' mapping", NULL);
    }
    json_t *m = tmt_tools_mod(ctx, "hermes_cli.plugins_cmd");
    if (tmt_is_raise(m)) { free(key); return m; }
    json_free(m);
    json_t *fa = tmt_args0();
    tmt_asets(fa, "key", key);
    json_t *found = tmt_call(ctx, "plugins_cmd._find_plugin_entry", fa);
    if (tmt_is_raise(found)) { free(key); return found; }
    if (!found || found->type == JSON_NULL) {
        size_t cap = strlen(key) + 32;
        char *msg = malloc(cap);
        if (msg) snprintf(msg, cap, "plugin '%s' not found", key);
        json_free(found);
        free(key);
        json_t *e = tmt_err(ctx, rid, 4020, msg ? msg : "", NULL);
        free(msg);
        return e;
    }
    if (found->type != JSON_ARRAY) {
        char msg[128];
        snprintf(msg, sizeof msg, "cannot unpack non-iterable %s object", tmt_type_name(found));
        json_free(found);
        free(key);
        return tmt_err_spec("TypeError", msg);
    }
    size_t fn = json_len(found);
    if (fn != 6) {
        char msg[128];
        snprintf(msg, sizeof msg, "%s values to unpack (expected 6, got %zu)",
                 fn < 6 ? "not enough" : "too many", fn);
        json_free(found);
        free(key);
        return tmt_err_spec("ValueError", msg);
    }
    const json_t *plugin_dir = json_get(found, 4);
    const json_t *canonical = json_get(found, 5);
    /* try: written = _tools_mod(...).save_plugin_settings(...)
     * except (ValueError, PermissionError) as e: return _err(rid, 4021, str(e)) */
    json_t *written = NULL;
    json_t *sm = tmt_tools_mod(ctx, "hermes_cli.plugins_settings");
    if (tmt_is_raise(sm)) {
        const char *kind = tmt_raise_kind(sm);
        if (!tmt_is_base_exc(kind) && (!strcmp(kind, "ValueError") || !strcmp(kind, "PermissionError"))) {
            json_t *e = tmt_err(ctx, rid, 4021, json_get_str(sm, "message", ""), NULL);
            json_free(sm);
            json_free(found);
            free(key);
            return e;
        }
        json_free(found);
        free(key);
        return sm;
    }
    json_free(sm);
    json_t *sa = tmt_args0();
    tmt_aset(sa, "canonical", canonical);
    if (tmt_truthy(plugin_dir)) {
        char *pds = tmt_pystr(plugin_dir);
        tmt_asets(sa, "plugin_dir", pds);
        free(pds);
    }
    tmt_aset(sa, "values", values);
    written = tmt_call(ctx, "plugins_settings.save_plugin_settings", sa);
    if (tmt_is_raise(written)) {
        const char *kind = tmt_raise_kind(written);
        if (!tmt_is_base_exc(kind) && (!strcmp(kind, "ValueError") || !strcmp(kind, "PermissionError"))) {
            json_t *e = tmt_err(ctx, rid, 4021, json_get_str(written, "message", ""), NULL);
            json_free(written);
            json_free(found);
            free(key);
            return e;
        }
        json_free(found);
        free(key);
        return written;
    }
    /* row = next((r for r in _plugin_rows() if r["key"] == canonical), None) */
    json_t *rows = tmt_call(ctx, "plugin_rows", tmt_args0());
    if (tmt_is_raise(rows)) { json_free(written); json_free(found); free(key); return rows; }
    if (!rows || rows->type != JSON_ARRAY) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", tmt_type_name(rows));
        json_free(rows);
        json_free(written);
        json_free(found);
        free(key);
        return tmt_err_spec("TypeError", msg);
    }
    const json_t *row = NULL;
    for (size_t i = 0; i < json_len(rows); i++) {
        json_t *rraise = NULL;
        const json_t *rk = tmt_row_get(json_get(rows, i), "key", &rraise);
        if (rraise) {
            json_free(rows);
            json_free(written);
            json_free(found);
            free(key);
            return rraise;
        }
        if (tmt_json_eq(rk, canonical)) { row = json_get(rows, i); break; }
    }
    json_t *em = tmt_tools_mod(ctx, "hermes_cli.plugin_events");
    if (tmt_is_raise(em)) {
        json_free(rows);
        json_free(written);
        json_free(found);
        free(key);
        return em;
    }
    json_free(em);
    const char *canonical_s = tmt_str(canonical);
    if (!canonical_s) {
        char msg[128];
        snprintf(msg, sizeof msg, "expected string or bytes-like object, got '%s'",
                 tmt_type_name(canonical));
        json_free(rows);
        json_free(written);
        json_free(found);
        free(key);
        return tmt_err_spec("TypeError", msg);
    }
    if (tmt_plugin_id_match(canonical_s)) {
        json_t *ba = tmt_args0();
        tmt_asets(ba, "canonical", canonical_s);
        tmt_asets(ba, "event", "settings.changed");
        json_t *data = json_object();
        tmt_aset(data, "written", written);
        json_set(ba, "data", data);
        json_t *br = tmt_call(ctx, "plugin_events.broadcast_plugin_event", ba);
        if (tmt_is_raise(br)) {
            json_free(rows);
            json_free(written);
            json_free(found);
            free(key);
            return br;
        }
        json_free(br);
    }
    if (tmt_truthy(plugin_dir)) {
        json_t *pa = tmt_args0();
        tmt_aset(pa, "plugin_dir", plugin_dir);
        json_t *portable = tmt_call(ctx, "plugins_cmd._is_portable_plugin_dir", pa);
        if (tmt_is_raise(portable)) {
            json_free(rows);
            json_free(written);
            json_free(found);
            free(key);
            return portable;
        }
        bool is_portable = tmt_truthy(portable);
        json_free(portable);
        if (is_portable) {
            json_t *pna = tmt_args0();
            char *pds = tmt_pystr(plugin_dir);
            tmt_asets(pna, "plugin_dir", pds);
            free(pds);
            tmt_aset(pna, "key", canonical);
            json_t *servers = tmt_call(ctx, "portable_server_names", pna);
            if (tmt_is_raise(servers)) {
                json_free(rows);
                json_free(written);
                json_free(found);
                free(key);
                return servers;
            }
            if (tmt_truthy(servers)) {
                json_t *dm = tmt_tools_mod(ctx, "tools.mcp_tool_discovery");
                if (tmt_is_raise(dm)) {
                    json_free(servers);
                    json_free(rows);
                    json_free(written);
                    json_free(found);
                    free(key);
                    return dm;
                }
                json_free(dm);
                json_t *sorted = tmt_sorted_str_set(servers);
                size_t cap = strlen(canonical_s) + 32;
                char *tname = malloc(cap);
                if (tname) snprintf(tname, cap, "plugin-settings-mcp-%s", canonical_s);
                json_t *spa = tmt_args0();
                tmt_asets(spa, "fn", "restart_mcp_servers_with_new_inputs");
                json_set(spa, "args", sorted);
                tmt_asets(spa, "name", tname ? tname : "");
                free(tname);
                json_t *th = tmt_call(ctx, "spawn_context_thread", spa);
                if (tmt_is_raise(th)) {
                    json_free(servers);
                    json_free(rows);
                    json_free(written);
                    json_free(found);
                    free(key);
                    return th;
                }
                json_free(th);
                json_t *st = tmt_call(ctx, "thread.start", tmt_args0());
                if (tmt_is_raise(st)) {
                    json_free(servers);
                    json_free(rows);
                    json_free(written);
                    json_free(found);
                    free(key);
                    return st;
                }
                json_free(st);
            }
            json_free(servers);
        }
    }
    json_t *payload = json_object();
    tmt_asetb(payload, "ok", true);
    tmt_aset(payload, "name", canonical);
    tmt_aset(payload, "written", written);
    json_set(payload, "plugin", row ? json_copy(row) : json_null());
    json_free(rows);
    json_free(written);
    json_free(found);
    free(key);
    return tmt_ok(ctx, rid, payload);
}

/* PoP: tmt_plugins_update_all @ tui_gateway/methods_tools.py:_plugins_update_all */
static json_t *tmt_plugins_update_all(tmt_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    json_t *r = tmt_call(ctx, "ensure_plugin_activation_listener", tmt_args0());
    if (tmt_is_raise(r)) return r;
    json_free(r);
    json_t *m = tmt_tools_mod(ctx, "hermes_cli.plugins_auto_update");
    if (tmt_is_raise(m)) return m;
    json_free(m);
    json_t *result = tmt_call(ctx, "plugins_auto_update.update_all_unattended", tmt_args0());
    if (tmt_is_raise(result)) return result;
    return tmt_ok(ctx, rid, result);
}

/* PoP: tmt_plugins_auto_update @ tui_gateway/methods_tools.py:_plugins_auto_update */
static json_t *tmt_plugins_auto_update(tmt_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    json_t *g = tmt_params_guard(params);
    if (g) return g;
    json_t *m = tmt_tools_mod(ctx, "hermes_cli.plugins_auto_update");
    if (tmt_is_raise(m)) return m;
    json_free(m);
    const json_t *enable = json_obj_get(params, "enable");
    if (!enable || enable->type == JSON_NULL) {
        json_t *result = tmt_call(ctx, "plugins_auto_update.auto_update_state", tmt_args0());
        if (tmt_is_raise(result)) return result;
        return tmt_ok(ctx, rid, result);
    }
    json_t *a = tmt_args0();
    tmt_asetb(a, "enable", tmt_truthy(enable));
    json_t *r = tmt_call(ctx, "plugins_auto_update.set_auto_update", a);
    if (tmt_is_raise(r)) {
        /* The except clause evaluates _tools_mod("hermes_cli.plugins_cmd")
         * .PluginOperationError for EVERY exception kind (BaseException
         * included) BEFORE the match check; a failure there replaces the
         * in-flight exception. The class exists only on the plugins_cmd
         * double — any other module marker raises AttributeError. */
        json_t *m2 = tmt_tools_mod(ctx, "hermes_cli.plugins_cmd");
        if (tmt_is_raise(m2)) { json_free(r); return m2; }
        const char *marker = tmt_str(m2);
        bool has_class = marker && strcmp(marker, "hermes_cli.plugins_cmd") == 0;
        json_free(m2);
        if (!has_class) {
            json_free(r);
            return tmt_err_spec("AttributeError",
                                "'types.SimpleNamespace' object has no attribute 'PluginOperationError'");
        }
        if (!strcmp(tmt_raise_kind(r), "PluginOperationError")) {
            json_t *e = tmt_err(ctx, rid, 4021, json_get_str(r, "message", ""), NULL);
            json_free(r);
            return e;
        }
        return r;
    }
    return tmt_ok(ctx, rid, r);
}

/* PoP: tmt_plugins_onboarding @ tui_gateway/methods_tools.py:_plugins_onboarding */
static json_t *tmt_plugins_onboarding(tmt_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    json_t *m = tmt_tools_mod(ctx, "hermes_cli.plugin_catalog_presence");
    if (tmt_is_raise(m)) return m;
    json_free(m);
    json_t *result = tmt_call(ctx, "plugin_catalog_presence.onboarding_entries", tmt_args0());
    if (tmt_is_raise(result)) return result;
    json_t *payload = json_object();
    json_set(payload, "onboarding", result);
    return tmt_ok(ctx, rid, payload);
}

/* PoP: tmt_plugins_presence @ tui_gateway/methods_tools.py:_plugins_presence */
static json_t *tmt_plugins_presence(tmt_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    json_t *g = tmt_params_guard(params);
    if (g) return g;
    json_t *m = tmt_tools_mod(ctx, "hermes_cli.plugin_catalog_presence");
    if (tmt_is_raise(m)) return m;
    json_free(m);
    const json_t *names = json_obj_get(params, "names");
    json_t *a = tmt_args0();
    json_set(a, "names", tmt_truthy(names) ? json_copy(names) : json_array());
    json_t *result = tmt_call(ctx, "plugin_catalog_presence.presence_for", a);
    if (tmt_is_raise(result)) return result;
    json_t *payload = json_object();
    json_set(payload, "presence", result);
    return tmt_ok(ctx, rid, payload);
}