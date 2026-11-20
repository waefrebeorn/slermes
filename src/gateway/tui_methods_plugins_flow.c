/*
 * tui_methods_plugins_flow.c — FLOW + leaf port of the TUI plugin-method
 * surface (hermes-agent). The quarry has NO tui_gateway/methods_plugins.py;
 * the real defining modules are the methods_*.py plugin family:
 *   tui_gateway/methods_plugin_catalog.py        (plugins.catalog.lookup)
 *   tui_gateway/methods_tools_plugin_commands.py (slash-command dispatch)
 *   tui_gateway/methods_tools_plugin_servers.py  (per-server rows/ownership)
 *   tui_gateway/methods_tools.py                 (plugin dispatch/refresh/rows)
 * PoP annotations credit the REAL defining module of every live def.
 *
 * Ports the DECISION SEQUENCES — branch order, seam call order, dict/set
 * state construction and exception paths — not just leaf outputs. Every live
 * external call enters through ONE universal seam: tmp_call(ctx, name, args).
 * The harness wires it to a recording stub fed the case's scripted results
 * (in.io); the oracle installs recording stubs emitting the identical event
 * shape {"name","args","ret"} from the SAME scripts.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and tmp_-prefixed.
 *
 * Server globals are seams: these bodies are rebound onto server.py's globals
 * at install time (method_ctx.bind_module), so _ok / _str_arg / _sessions /
 * _sessions_lock / _set_session_context / _clear_session_context /
 * _session_source / _session_profile_runtime_scope / _profile_home /
 * _load_enabled_toolsets / _load_disabled_toolsets / get_process_hermes_home
 * / logger / _emit / _session_info / bind_module are bare free variables.
 * Claimed siblings (_ok server.py, _str_arg methods_tools.py, _clarify_block
 * server.py) are seamed, never re-ported. Ported siblings here seam each
 * other (each is exercised by its own cases): _tools_mod,
 * _plugin_command_handler, _run_plugin_command,
 * _dispatch_plugin_in_profile_scope, _is_registry_command, _server_identity,
 * _portable_server_tools, _plugin_tool_ownership, _plugin_server_rows,
 * _refresh_live_sessions, _session_home_scope (CM usage).
 *
 * Seam name -> live call (args are informational projections):
 *   tools_mod                   _tools_mod(module) -> module marker string
 *   importlib.import_module     importlib.import_module(module) inside the
 *                               REAL _tools_mod (in-function import; the
 *                               import itself is silent)
 *   ok                          _ok(rid, result)  [claimed]
 *   str_arg                     _str_arg(params, key)  [claimed]
 *   bind_module                 bind_module(globals(), server, skip=...)
 *                               [globals() projects to the module __name__]
 *   plugin_catalog.load_catalog_live / resolved_removed_entries /
 *   match_removed               hermes_cli.plugin_catalog in-function imports
 *   plugins_cmd_catalog.looks_like_catalog_name
 *                               hermes_cli.plugins_cmd_catalog in-function
 *                               import (catalog handler)
 *   plugins.get_plugin_command_handler / plugin_command_absent_nowait /
 *   resolve_plugin_command_result / discover_plugins
 *                               hermes_cli.plugins module-double methods
 *   get_plugin_manager          hermes_cli.plugins.get_plugin_manager()
 *   plugin_command_dispatch.invoke_plugin_command(handler, arg, **context)
 *                               [context source=None key dropped]
 *   plugin_choices.clarify_asker(lambda questions: _clarify_block(sid, ...))
 *                               [callback projects to "_clarify_block"]
 *   plugin_choices.bind_asker(asker) -> silent CM
 *   set_session_context         _set_session_context(key, cwd=...)
 *   clear_session_context       _clear_session_context(tokens)
 *   session_source              _session_source(session)
 *   session_home_scope          _session_home_scope(session, profile=...)
 *                               -> silent CM (enter/exit untraced)
 *   session_profile_runtime_scope  _session_profile_runtime_scope(session)
 *                               -> silent CM (enter/exit untraced)
 *   profile_home                _profile_home(profile) -> POSIX str | null
 *   is_registry_command         _is_registry_command(name)  [ported sibling]
 *   plugin_command_handler      _plugin_command_handler(name)  [sibling]
 *   run_plugin_command          _run_plugin_command(handler, arg, session, sid)
 *   dispatch_plugin_in_profile_scope(name, arg, session, sid, profile)
 *   commands.resolve_command    hermes_cli.commands.resolve_command(base)
 *   agent_plugins.load_agent_plugin(plugin_dir, root)  [same path twice]
 *   plugins_manifest.portable_mcp_server_name(key, server)
 *   mcp_liveness.status(internal_name) / describe(decl, availability, state,
 *                               display_name)
 *   declaration.lookup          hermes_platform.declaration.lookup(name)
 *   core._servers.get(key)      tools.mcp_tool_common._core._servers.get
 *   core._lazy_server_tool_names.get(key)
 *   server._is_recycled_stdio   the server double's method (args carry id)
 *   mcp_tool_scope._resolve_server_key(server)
 *   mcp_tool_config._interpolate_env_vars(cfg)
 *   server_identity             _server_identity(cfg)  [ported sibling]
 *   manager.get_portable_mcp_servers / get_portable_mcp_server_plugins
 *   portable_server_tools       _portable_server_tools(manager, native)
 *   config.load_config          hermes_cli.config.load_config()
 *   plugin_tool_ownership       _plugin_tool_ownership()  [ported sibling]
 *   plugins_cmd._get_enabled_set / _get_disabled_set / _read_install_metadata
 *                               / _category_active_names /
 *                               _discover_all_plugins / _plugin_status /
 *                               _is_portable_plugin_dir / pinned_revision
 *   plugins_cmd_catalog.catalog_rows_maps / catalog_install_record /
 *                               catalog_row_fields
 *   plugins_cadence.cached_git_updates(ref_pins)
 *   desktop_plugin_halves.desktop_source(plugin_dir)
 *   plugins_settings.plugin_settings_fields(key, plugin_dir)
 *   plugin_server_rows          _plugin_server_rows(dir, key, portable=...,
 *                               catalog_titles=...)  [ported sibling]
 *   refresh_live_sessions       _refresh_live_sessions(home,
 *                               preserve_prefix=..., plugin_note=...)
 *   sessions_items              list(_sessions.items()) -> [[sid, sess], ...]
 *                               (_sessions_lock is a SILENT lock: no events)
 *   hermes_constants.hermes_home_key(home)  [in-function import]
 *   get_process_hermes_home     server global
 *   mcp_tool_agent.refresh_agent_mcp_tools(agent, enabled_override=...,
 *                               disabled_override=..., quiet_mode=True,
 *                               preserve_prefix=...)
 *   carry_onboarding_toolset(enabled, agent)  [in-function import]
 *   load_enabled_toolsets(platform) / load_disabled_toolsets()
 *   logger.warning(fmt, sid, exc)  [args project to the formatted message]
 *   plugin_live_notes.queue_plugin_notice(agent, key, text)
 *   session_info                _session_info(agent, session)
 *   emit                        _emit("session.info", sid, info)
 *   hermes_constants.set_hermes_home_override / reset_hermes_home_override
 *   agent.runtime_cwd.set_session_cwd / reset_session_cwd
 *
 * Module DATA attributes (no call, no trace): io["toolsets.data"] scripts
 * {_HERMES_CORE_TOOLS: [...], TOOLSETS: {...}} for the toolsets module double.
 *
 * Representation contract (shared with tests/oracle_tui_methods_plugins_flow.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)}; BaseException-only
 *   kinds (SystemExit, KeyboardInterrupt, GeneratorExit, CancelledError) PASS
 *   THROUGH every except-Exception / suppress(Exception) site.
 * - Scripted raises of plain kinds carry str(exc) == message verbatim (the
 *   oracle materializes real exception instances).
 * - Module doubles: tools_mod scripts return the module NAME string; the
 *   oracle resolves it to a SimpleNamespace of recording stubs. A wrong
 *   marker raises AttributeError on the first attribute access, exactly as
 *   the SimpleNamespace double does.
 * - CM doubles are SILENT: the construction seam fires once; __enter__ /
 *   __exit__ emit no events (a construction raise models the bind failure).
 *   _sessions_lock is silent too.
 * - Agent doubles: session["agent"] dicts materialize to SimpleNamespaces on
 *   the PY side; seam args project them back to the raw dict (C carries the
 *   raw JSON throughout). Server doubles carry "id" + "session" (required
 *   keys); status doubles carry "state" + "availability"; package doubles
 *   carry "manifest"; entry doubles carry name/repo/sha/subdir/tier.
 * - Tuples project to lists; sets to sorted unique lists; integral floats to
 *   ints; Paths to POSIX strings. The oracle constructs PurePosixPath so
 *   str(Path) keeps forward slashes on Windows; fixture paths are POSIX,
 *   normalized (no "//", "./", trailing "/"), no drive letters/UNC.
 * - str()/strip() fold the ASCII fixture corpus only; str()-projected values
 *   are strings/ints/bools/null (NO floats: str(5.0) != str(5)).
 * - sorted(_discover_all_plugins()) tuples are all-string 6-tuples with
 *   unique names (total order; qsort stability is not exercised).
 * - Seam args drop None-valued TOP-LEVEL keys on both sides (absent-vs-null
 *   parity); nested nulls are preserved.
 * - State: no ported function mutates module globals; out.state echoes
 *   in.state (the harness keeps the field for shape uniformity).
 *
 * Ownership: tmp_call ADOPTS args into the trace event; its return is a fresh
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
typedef struct tmp_ctx {
    json_t *state;      /* owned; echoed as out.state (no mutations here) */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} tmp_ctx_t;

/* ── basic helpers ── */
static bool tmp_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static const char *tmp_raise_kind(const json_t *v) {
    return tmp_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}

/* BaseException-only kinds pass through every except-Exception site. */
static bool tmp_is_base_exc(const char *kind) {
    return kind && (!strcmp(kind, "SystemExit") || !strcmp(kind, "KeyboardInterrupt")
                    || !strcmp(kind, "GeneratorExit") || !strcmp(kind, "CancelledError"));
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool tmp_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static const char *tmp_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static json_t *tmp_err_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* Python type name for a JSON value (fixture scalar/container domain). */
static const char *tmp_type_name(const json_t *v) {
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

/* Python str() for scalars. Fixture domain: str/int/bool/null only (integral
 * doubles print as ints; non-integral floats are outside the str() domain). */
static char *tmp_pystr(const json_t *v) {
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

static char *tmp_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

static bool tmp_is_ascii_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* str.strip(): ASCII whitespace both ends (fixtures fold ASCII only). */
static char *tmp_strip(const char *s) {
    if (!s) s = "";
    while (*s && tmp_is_ascii_ws((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && tmp_is_ascii_ws((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - s);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

/* Deep scalar equality (Python == on the fixture domain; bool==int cross). */
static bool tmp_json_eq(const json_t *a, const json_t *b) {
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
            if (!tmp_json_eq(a->c.items[i], b->c.items[i])) return false;
        return true;
    }
    if (a->type == JSON_OBJECT) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !tmp_json_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}

/* ── the one universal seam (fail-closed) ── */
static json_t *tmp_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return tmp_err_spec(type, msg);
}

static json_t *tmp_call(tmp_ctx_t *ctx, const char *name, json_t *args) {
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
        ret = tmp_err_spec("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = tmp_err_spec("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = tmp_make_exc(raise);
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
static json_t *tmp_args0(void) { return json_object(); }
static void tmp_aset(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static void tmp_asets(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void tmp_asetb(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void tmp_asetn(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }

/* _tools_mod(module): deferred import seam; returns the module marker. */
static json_t *tmp_tools_mod_seam(tmp_ctx_t *ctx, const char *module) {
    json_t *a = tmp_args0();
    tmp_asets(a, "module", module);
    return tmp_call(ctx, "tools_mod", a);
}

/* _ok(rid, result) — result is ADOPTED. */
static json_t *tmp_ok(tmp_ctx_t *ctx, const json_t *rid, json_t *result) {
    json_t *a = tmp_args0();
    tmp_aset(a, "rid", rid);
    json_set(a, "result", result ? result : json_null());
    return tmp_call(ctx, "ok", a);
}

/* Module-double attribute gate: a wrong tools_mod marker raises
 * AttributeError on the first attribute access (SimpleNamespace double). */
static json_t *tmp_mod_check(const json_t *marker, const char *expected,
                             const char *attr) {
    const char *m = tmp_str(marker);
    if (m && strcmp(m, expected) == 0) return NULL;
    char msg[192];
    snprintf(msg, sizeof msg,
             "'types.SimpleNamespace' object has no attribute '%s'", attr);
    return tmp_err_spec("AttributeError", msg);
}

/* Module DATA attributes: io["<module>.data"], read WITHOUT a trace event. */
static const json_t *tmp_module_data(tmp_ctx_t *ctx, const char *module) {
    if (!ctx->io) return NULL;
    char key[128];
    snprintf(key, sizeof key, "%s.data", module);
    return json_obj_get(ctx->io, key);
}

/* params.get(key, default) with the live AttributeError surface for a
 * non-dict params. Returns a BORROWED node (NULL == absent -> default). */
static const json_t *tmp_params_get(const json_t *params, const char *key,
                                    json_t **raise_out) {
    *raise_out = NULL;
    if (!params || params->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'",
                 tmp_type_name(params));
        *raise_out = tmp_err_spec("AttributeError", msg);
        return NULL;
    }
    return json_obj_get(params, key);
}

/* ── sorted-unique string sets (model Python sets; canon sorts) ── */
static int tmp_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static bool tmp_set_contains(const json_t *set, const char *s) {
    if (!set || set->type != JSON_ARRAY || !s) return false;
    for (size_t i = 0; i < json_len(set); i++) {
        const char *x = tmp_str(json_get(set, i));
        if (x && strcmp(x, s) == 0) return true;
    }
    return false;
}

/* fresh sorted-unique array from a list of strings (non-strs skipped) */
static json_t *tmp_set_norm(const json_t *list) {
    json_t *out = json_array();
    if (!list || list->type != JSON_ARRAY) return out;
    size_t n = json_len(list);
    const char **vals = malloc((n + 1) * sizeof(char *));
    if (!vals) return out;
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        const char *s = tmp_str(json_get(list, i));
        if (s) vals[k++] = s;
    }
    qsort(vals, k, sizeof(char *), tmp_cmp_str);
    for (size_t i = 0; i < k; i++) {
        if (i > 0 && strcmp(vals[i], vals[i - 1]) == 0) continue;
        json_append(out, json_string(vals[i]));
    }
    free(vals);
    return out;
}

/* in-place sorted-unique add */
static void tmp_set_add(json_t *set, const char *s) {
    if (!set || set->type != JSON_ARRAY || !s) return;
    if (tmp_set_contains(set, s)) return;
    size_t n = json_len(set);
    const char **vals = malloc((n + 1) * sizeof(char *));
    if (!vals) return;
    for (size_t i = 0; i < n; i++) vals[i] = tmp_str(json_get(set, i));
    vals[n] = s;
    qsort(vals, n + 1, sizeof(char *), tmp_cmp_str);
    json_t *fresh = json_array();
    for (size_t i = 0; i < n + 1; i++)
        json_append(fresh, json_string(vals[i] ? vals[i] : ""));
    free(vals);
    /* replace contents in place (borrowed pointers to set stay valid) */
    for (size_t i = 0; i < n; i++) json_free(set->c.items[i]);
    free(set->c.items);
    set->c.items = fresh->c.items;
    set->c.count = fresh->c.count;
    fresh->c.items = NULL;
    fresh->c.count = 0;
    json_free(fresh);
}

static void tmp_set_update(json_t *dst, const json_t *src) {
    if (!src || src->type != JSON_ARRAY) return;
    for (size_t i = 0; i < json_len(src); i++) {
        const char *s = tmp_str(json_get(src, i));
        if (s) tmp_set_add(dst, s);
    }
}

/* a - b (fresh array) */
static json_t *tmp_set_diff(const json_t *a, const json_t *b) {
    json_t *out = json_array();
    if (!a || a->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < json_len(a); i++) {
        const char *s = tmp_str(json_get(a, i));
        if (s && !tmp_set_contains(b, s)) json_append(out, json_string(s));
    }
    return out;
}

/* ── POSIX path algebra (PurePosixPath domain; fixture paths normalized) ── */
/* str(PurePosixPath(s)): collapse "//", drop "." components and trailing
 * "/", keep ".." (Path does not resolve it). "" -> ".". */
static char *tmp_path_norm(const char *s) {
    if (!s) s = "";
    bool absolute = s[0] == '/';
    size_t n = strlen(s);
    char *buf = malloc(n + 2);
    if (!buf) return NULL;
    size_t k = 0;
    size_t i = 0;
    while (i < n) {
        while (i < n && s[i] == '/') i++;
        size_t start = i;
        while (i < n && s[i] != '/') i++;
        size_t len = i - start;
        if (len == 0) continue;
        if (len == 1 && s[start] == '.') continue;
        if (k > 0) buf[k++] = '/';
        memcpy(buf + k, s + start, len);
        k += len;
    }
    buf[k] = '\0';
    char *out;
    if (absolute) {
        out = malloc(k + 2);
        if (!out) { free(buf); return NULL; }
        out[0] = '/';
        memcpy(out + 1, buf, k + 1);
    } else if (k == 0) {
        out = tmp_strdup(".");
    } else {
        out = tmp_strdup(buf);
    }
    free(buf);
    return out;
}

/* PurePosixPath.name: the final component ("" for "/"). */
static char *tmp_path_name(const char *norm) {
    if (!norm) return tmp_strdup("");
    const char *slash = strrchr(norm, '/');
    if (!slash) return tmp_strdup(norm);
    return tmp_strdup(slash + 1);
}

/* ── sorting (Python sorted() on the fixture domains) ── */
static int tmp_cmp_json_str(const void *a, const void *b) {
    const json_t *ja = *(const json_t *const *)a;
    const json_t *jb = *(const json_t *const *)b;
    const char *sa = tmp_str(ja), *sb = tmp_str(jb);
    return strcmp(sa ? sa : "", sb ? sb : "");
}

/* sorted(list_of_string_tuples): lexicographic, element-wise strcmp;
 * a proper prefix sorts first. All-string domain (documented). */
static int tmp_cmp_tuple(const void *a, const void *b) {
    const json_t *ta = *(const json_t *const *)a;
    const json_t *tb = *(const json_t *const *)b;
    size_t na = json_len(ta), nb = json_len(tb);
    size_t n = na < nb ? na : nb;
    for (size_t i = 0; i < n; i++) {
        const char *sa = tmp_str(json_get(ta, i));
        const char *sb = tmp_str(json_get(tb, i));
        int c = strcmp(sa ? sa : "", sb ? sb : "");
        if (c) return c;
    }
    return (na > nb) - (na < nb);
}

static json_t *tmp_sorted_strings(const json_t *arr) {
    json_t *out = json_array();
    if (!arr || arr->type != JSON_ARRAY) return out;
    size_t n = json_len(arr);
    const json_t **vals = malloc((n + 1) * sizeof(json_t *));
    if (!vals) return out;
    for (size_t i = 0; i < n; i++) vals[i] = json_get(arr, i);
    qsort(vals, n, sizeof(json_t *), tmp_cmp_json_str);
    for (size_t i = 0; i < n; i++) json_append(out, json_copy(vals[i]));
    free(vals);
    return out;
}

static json_t *tmp_sorted_tuples(const json_t *arr) {
    json_t *out = json_array();
    if (!arr || arr->type != JSON_ARRAY) return out;
    size_t n = json_len(arr);
    const json_t **vals = malloc((n + 1) * sizeof(json_t *));
    if (!vals) return out;
    for (size_t i = 0; i < n; i++) vals[i] = json_get(arr, i);
    qsort(vals, n, sizeof(json_t *), tmp_cmp_tuple);
    for (size_t i = 0; i < n; i++) json_append(out, json_copy(vals[i]));
    free(vals);
    return out;
}

/* dict key list (insertion order) */
static json_t *tmp_obj_keys(const json_t *obj) {
    json_t *out = json_array();
    if (!obj || obj->type != JSON_OBJECT) return out;
    for (size_t i = 0; i < obj->c.count; i++)
        json_append(out, json_string(obj->c.keys[i]));
    return out;
}
/* ── attribute / .get surfaces ── */
/* obj.attr on a SimpleNamespace double: missing key -> AttributeError. */
static json_t *tmp_ns_attr(const json_t *obj, const char *attr, const json_t **out) {
    *out = NULL;
    if (!obj || obj->type != JSON_OBJECT) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'",
                 tmp_type_name(obj), attr);
        return tmp_err_spec("AttributeError", msg);
    }
    const json_t *v = json_obj_get(obj, attr);
    if (!v) {
        char msg[160];
        snprintf(msg, sizeof msg,
                 "'types.SimpleNamespace' object has no attribute '%s'", attr);
        return tmp_err_spec("AttributeError", msg);
    }
    *out = v;
    return NULL;
}

/* v.get(key) with the live AttributeError surface for a non-dict v.
 * Returns a BORROWED node (NULL == absent). */
static const json_t *tmp_dict_get_v(const json_t *v, const char *key,
                                    json_t **raise_out) {
    *raise_out = NULL;
    if (!v || v->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'",
                 tmp_type_name(v));
        *raise_out = tmp_err_spec("AttributeError", msg);
        return NULL;
    }
    return json_obj_get(v, key);
}

/* str(v or "") — always a malloc'd string. */
static char *tmp_str_or_empty(const json_t *v) {
    char *s = tmp_truthy(v) ? tmp_pystr(v) : tmp_strdup("");
    return s;
}

/* ── ported functions ── */

/* PoP: tmp_tools_mod @ tui_gateway/methods_tools.py:_tools_mod */
static json_t *tmp_tools_mod(tmp_ctx_t *ctx, const json_t *args) {
    const char *module = json_get_str(args, "module", "");
    json_t *a = tmp_args0();
    tmp_asets(a, "module", module);
    return tmp_call(ctx, "importlib.import_module", a);
}

/* PoP: tmp_is_registry_command @ tui_gateway/methods_tools.py:_is_registry_command */
static json_t *tmp_is_registry_command(tmp_ctx_t *ctx, const json_t *args) {
    const char *base = json_get_str(args, "base", "");
    json_t *m = tmp_tools_mod_seam(ctx, "hermes_cli.commands");
    if (tmp_is_raise(m)) {
        if (tmp_is_base_exc(tmp_raise_kind(m))) return m;
        json_free(m);
        return json_bool(false);
    }
    json_t *bad = tmp_mod_check(m, "hermes_cli.commands", "resolve_command");
    json_free(m);
    if (bad) { json_free(bad); return json_bool(false); }
    json_t *a = tmp_args0();
    tmp_asets(a, "base", base);
    json_t *r = tmp_call(ctx, "commands.resolve_command", a);
    if (tmp_is_raise(r)) {
        if (tmp_is_base_exc(tmp_raise_kind(r))) return r;
        json_free(r);
        return json_bool(false);
    }
    bool hit = r && r->type != JSON_NULL;
    json_free(r);
    return json_bool(hit);
}

/* PoP: tmp_plugin_command_handler @ tui_gateway/methods_tools_plugin_commands.py:_plugin_command_handler */
static json_t *tmp_plugin_command_handler(tmp_ctx_t *ctx, const json_t *args) {
    const char *name = json_get_str(args, "name", "");
    json_t *m = tmp_tools_mod_seam(ctx, "hermes_cli.plugins");
    if (tmp_is_raise(m)) {
        if (tmp_is_base_exc(tmp_raise_kind(m))) return m;
        json_free(m);
        return NULL;
    }
    json_t *bad = tmp_mod_check(m, "hermes_cli.plugins", "get_plugin_command_handler");
    json_free(m);
    if (bad) { json_free(bad); return NULL; }
    json_t *a = tmp_args0();
    tmp_asets(a, "name", name);
    json_t *r = tmp_call(ctx, "plugins.get_plugin_command_handler", a);
    if (tmp_is_raise(r)) {
        if (tmp_is_base_exc(tmp_raise_kind(r))) return r;
        json_free(r);
        return NULL;
    }
    return r;
}

/* PoP: tmp_catalog_lookup @ tui_gateway/methods_plugin_catalog.py:_ */
static json_t *tmp_catalog_lookup(tmp_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    json_t *raise = NULL;
    const json_t *name_v = tmp_params_get(params, "name", &raise);
    if (raise) return raise;
    char *raw = tmp_str_or_empty(name_v);
    char *name = tmp_strip(raw);
    free(raw);
    /* in-function imports are silent (stub modules); the calls are seams */
    json_t *la = tmp_args0();
    tmp_asets(la, "name", name);
    json_t *looks = tmp_call(ctx, "plugins_cmd_catalog.looks_like_catalog_name", la);
    if (tmp_is_raise(looks)) { free(name); return looks; }
    bool valid = tmp_truthy(looks);
    json_free(looks);
    if (!valid) {
        free(name);
        json_t *p = json_object();
        json_set_hdr(p, "status", "invalid_name");
        return tmp_ok(ctx, rid, p);
    }
    json_t *entries = tmp_call(ctx, "plugin_catalog.load_catalog_live", tmp_args0());
    if (tmp_is_raise(entries)) { free(name); return entries; }
    if (!tmp_truthy(entries)) {
        json_free(entries);
        free(name);
        json_t *p = json_object();
        json_set_hdr(p, "status", "unavailable");
        return tmp_ok(ctx, rid, p);
    }
    /* entry = next((e for e in entries if e.name == name), None) */
    const json_t *entry = NULL;
    json_t *iter_raise = NULL;
    if (entries->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(entries); i++) {
            const json_t *e = json_get(entries, i);
            const json_t *en = NULL;
            json_t *bad = tmp_ns_attr(e, "name", &en);
            if (bad) { iter_raise = bad; break; }
            const char *ens = tmp_str(en);
            if (ens && strcmp(ens, name) == 0) { entry = e; break; }
        }
    } else if (entries->type == JSON_OBJECT) {
        /* iterating a dict yields its str keys: e.name raises */
        if (entries->c.count > 0)
            iter_raise = tmp_err_spec("AttributeError",
                                      "'str' object has no attribute 'name'");
    } else {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", tmp_type_name(entries));
        iter_raise = tmp_err_spec("TypeError", msg);
    }
    if (iter_raise) { json_free(entries); free(name); return iter_raise; }
    json_t *removed = NULL;
    if (entry) {
        removed = tmp_call(ctx, "plugin_catalog.resolved_removed_entries", tmp_args0());
        if (tmp_is_raise(removed)) { json_free(entries); free(name); return removed; }
    } else {
        removed = json_array();
    }
    bool unknown = false;
    if (!entry) {
        unknown = true;
    } else {
        const json_t *en = NULL, *er = NULL;
        json_t *bad = tmp_ns_attr(entry, "name", &en);
        if (!bad) {
            json_t *ma = tmp_args0();
            tmp_aset(ma, "name", en);
            tmp_aset(ma, "removed", removed);
            json_t *m1 = tmp_call(ctx, "plugin_catalog.match_removed", ma);
            if (tmp_is_raise(m1)) {
                json_free(removed); json_free(entries); free(name);
                return m1;
            }
            bool hit1 = tmp_truthy(m1);
            json_free(m1);
            if (hit1) {
                unknown = true;
            } else {
                bad = tmp_ns_attr(entry, "repo", &er);
                if (!bad) {
                    json_t *ma2 = tmp_args0();
                    tmp_aset(ma2, "name", er);
                    tmp_aset(ma2, "removed", removed);
                    json_t *m2 = tmp_call(ctx, "plugin_catalog.match_removed", ma2);
                    if (tmp_is_raise(m2)) {
                        json_free(removed); json_free(entries); free(name);
                        return m2;
                    }
                    unknown = tmp_truthy(m2);
                    json_free(m2);
                }
            }
        }
        if (bad) { json_free(removed); json_free(entries); free(name); return bad; }
    }
    json_free(removed);
    if (unknown) {
        json_free(entries);
        free(name);
        json_t *p = json_object();
        json_set_hdr(p, "status", "unknown");
        return tmp_ok(ctx, rid, p);
    }
    /* found payload: name/repo/sha/subdir/tier (all required ns attrs) */
    json_t *p = json_object();
    json_set_hdr(p, "status", "found");
    json_t *ed = json_object();
    const char *fields[5] = { "name", "repo", "sha", "subdir", "tier" };
    for (int i = 0; i < 5; i++) {
        const json_t *fv = NULL;
        json_t *bad = tmp_ns_attr(entry, fields[i], &fv);
        if (bad) {
            json_free(ed); json_free(p);
            json_free(entries); free(name);
            return bad;
        }
        json_set(ed, fields[i], json_copy(fv));
    }
    json_set(p, "entry", ed);
    json_free(entries);
    free(name);
    return tmp_ok(ctx, rid, p);
}

/* bind_module(globals(), server, skip=...) — globals() projects to __name__. */
static json_t *tmp_register(tmp_ctx_t *ctx, const json_t *args,
                            const char *module, bool skip_underscore) {
    const json_t *server = json_obj_get(args, "server");
    json_t *a = tmp_args0();
    tmp_asets(a, "module", module);
    tmp_aset(a, "server", server);
    json_t *skip = json_array();
    if (skip_underscore) json_append(skip, json_string("_"));
    json_set(a, "skip", skip);
    json_t *r = tmp_call(ctx, "bind_module", a);
    if (tmp_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* PoP: tmp_catalog_register @ tui_gateway/methods_plugin_catalog.py:register */
static json_t *tmp_catalog_register(tmp_ctx_t *ctx, const json_t *args) {
    return tmp_register(ctx, args, "tui_gateway.methods_plugin_catalog", true);
}

/* PoP: tmp_plugin_commands_register @ tui_gateway/methods_tools_plugin_commands.py:register */
static json_t *tmp_plugin_commands_register(tmp_ctx_t *ctx, const json_t *args) {
    return tmp_register(ctx, args, "tui_gateway.methods_tools_plugin_commands", false);
}

/* PoP: tmp_plugin_servers_register @ tui_gateway/methods_tools_plugin_servers.py:register */
static json_t *tmp_plugin_servers_register(tmp_ctx_t *ctx, const json_t *args) {
    return tmp_register(ctx, args, "tui_gateway.methods_tools_plugin_servers", false);
}

/* PoP: tmp_server_identity @ tui_gateway/methods_tools_plugin_servers.py:_server_identity */
static json_t *tmp_server_identity(tmp_ctx_t *ctx, const json_t *args) {
    const json_t *cfg_in = json_obj_get(args, "cfg");
    json_t *empty = json_object();
    const json_t *inter = (cfg_in && cfg_in->type == JSON_OBJECT) ? cfg_in : empty;
    json_t *m = tmp_tools_mod_seam(ctx, "tools.mcp_tool_config");
    if (tmp_is_raise(m)) { json_free(empty); return m; }
    json_t *bad = tmp_mod_check(m, "tools.mcp_tool_config", "_interpolate_env_vars");
    json_free(m);
    if (bad) { json_free(empty); return bad; }
    json_t *a = tmp_args0();
    tmp_aset(a, "cfg", inter);
    json_t *cfg = tmp_call(ctx, "mcp_tool_config._interpolate_env_vars", a);
    json_free(empty);
    if (tmp_is_raise(cfg)) return cfg;
    json_t *raise = NULL;
    const json_t *args_v = tmp_dict_get_v(cfg, "args", &raise);
    if (raise) { json_free(cfg); return raise; }
    const json_t *cmd_v = tmp_dict_get_v(cfg, "command", &raise);
    if (raise) { json_free(cfg); return raise; }
    const json_t *url_v = tmp_dict_get_v(cfg, "url", &raise);
    if (raise) { json_free(cfg); return raise; }
    char *cmd = tmp_str_or_empty(cmd_v);
    char *url = tmp_str_or_empty(url_v);
    json_t *out = json_array();
    json_append(out, json_string(cmd));
    free(cmd);
    json_t *argl = json_array();
    if (tmp_truthy(args_v) && args_v->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(args_v); i++) {
            char *s = tmp_pystr(json_get(args_v, i));
            json_append(argl, json_string(s));
            free(s);
        }
    } else if (tmp_truthy(args_v)) {
        char *s = tmp_pystr(args_v);
        json_append(argl, json_string(s));
        free(s);
    } else {
        /* args = cfg.get("args") or () -> the empty TUPLE is not a list,
         * so the live comprehension wraps it: [()] -> str(()) == "()" */
        json_append(argl, json_string("()"));
    }
    json_append(out, argl);
    json_append(out, json_string(url));
    free(url);
    json_free(cfg);
    return out;
}

/*
 * The live def is a @contextlib.contextmanager generator. Standalone drive =
 * next(gen) (enter phase up to yield) + gen.close() (the finally resets).
 * A raise BEFORE the try (override/cwd binding) skips the resets entirely.
 */
/* PoP: tmp_session_home_scope @ tui_gateway/methods_tools.py:_session_home_scope */
static json_t *tmp_session_home_scope(tmp_ctx_t *ctx, const json_t *args) {
    const json_t *session = json_obj_get(args, "session");
    const json_t *cwd_arg = json_obj_get(args, "cwd");
    const json_t *profile = json_obj_get(args, "profile");
    json_t *m1 = tmp_tools_mod_seam(ctx, "hermes_constants");
    if (tmp_is_raise(m1)) return m1;
    json_t *m2 = tmp_tools_mod_seam(ctx, "agent.runtime_cwd");
    if (tmp_is_raise(m2)) { json_free(m1); return m2; }
    json_t *raise = NULL;
    const json_t *ph_v = NULL;
    if (tmp_truthy(session)) {
        ph_v = tmp_dict_get_v(session, "profile_home", &raise);
        if (raise) { json_free(m1); json_free(m2); return raise; }
    }
    char *profile_home = NULL;
    if (ph_v && ph_v->type != JSON_NULL) {
        /* session-bound profile_home is used verbatim (str domain) */
        const char *s = tmp_str(ph_v);
        if (s && s[0]) profile_home = tmp_strdup(s);
    }
    if (!tmp_truthy(session) && tmp_truthy(profile)) {
        json_t *pa = tmp_args0();
        tmp_aset(pa, "profile", profile);
        json_t *ph = tmp_call(ctx, "profile_home", pa);
        if (tmp_is_raise(ph)) { json_free(m1); json_free(m2); return ph; }
        char *s = tmp_str_or_empty(ph);
        json_free(ph);
        if (s[0]) profile_home = s; else free(s);
    }
    char *cwd = NULL;
    if (tmp_truthy(cwd_arg)) {
        const char *s = tmp_str(cwd_arg);
        cwd = tmp_strdup(s ? s : "");
    } else if (tmp_truthy(session)) {
        const json_t *cv = tmp_dict_get_v(session, "cwd", &raise);
        if (raise) {
            free(profile_home); json_free(m1); json_free(m2); return raise;
        }
        cwd = tmp_str_or_empty(cv);
    } else {
        cwd = tmp_strdup("");
    }
    json_t *token = NULL;
    if (profile_home) {
        json_t *bad = tmp_mod_check(m1, "hermes_constants", "set_hermes_home_override");
        if (bad) { free(profile_home); free(cwd); json_free(m1); json_free(m2); return bad; }
        json_t *ha = tmp_args0();
        tmp_asets(ha, "profile_home", profile_home);
        token = tmp_call(ctx, "hermes_constants.set_hermes_home_override", ha);
        if (tmp_is_raise(token)) {
            free(profile_home); free(cwd); json_free(m1); json_free(m2);
            return token;
        }
    }
    json_t *cwd_token = NULL;
    if (cwd[0]) {
        json_t *bad = tmp_mod_check(m2, "agent.runtime_cwd", "set_session_cwd");
        if (bad) {
            json_free(token); free(profile_home); free(cwd);
            json_free(m1); json_free(m2); return bad;
        }
        json_t *ca = tmp_args0();
        tmp_asets(ca, "cwd", cwd);
        cwd_token = tmp_call(ctx, "agent.runtime_cwd.set_session_cwd", ca);
        if (tmp_is_raise(cwd_token)) {
            /* before the try: the home token is NOT reset */
            json_free(token); free(profile_home); free(cwd);
            json_free(m1); json_free(m2);
            return cwd_token;
        }
    }
    free(profile_home);
    free(cwd);
    /* yield (empty body in the standalone drive), then the finally resets */
    if (cwd_token && cwd_token->type != JSON_NULL) {
        json_t *bad = tmp_mod_check(m2, "agent.runtime_cwd", "reset_session_cwd");
        if (bad) {
            json_free(cwd_token); json_free(token); json_free(m1); json_free(m2);
            return bad;
        }
        json_t *ra = tmp_args0();
        tmp_aset(ra, "token", cwd_token);
        json_t *r = tmp_call(ctx, "agent.runtime_cwd.reset_session_cwd", ra);
        if (tmp_is_raise(r)) {
            json_free(cwd_token); json_free(token); json_free(m1); json_free(m2);
            return r;
        }
        json_free(r);
    }
    if (token && token->type != JSON_NULL) {
        json_t *bad = tmp_mod_check(m1, "hermes_constants", "reset_hermes_home_override");
        if (bad) {
            json_free(cwd_token); json_free(token); json_free(m1); json_free(m2);
            return bad;
        }
        json_t *ra = tmp_args0();
        tmp_aset(ra, "token", token);
        json_t *r = tmp_call(ctx, "hermes_constants.reset_hermes_home_override", ra);
        if (tmp_is_raise(r)) {
            json_free(cwd_token); json_free(token); json_free(m1); json_free(m2);
            return r;
        }
        json_free(r);
    }
    json_free(cwd_token);
    json_free(token);
    json_free(m1);
    json_free(m2);
    return NULL;
}

/*
 * Pre-try computation (modules/key/tokens/context/choices) propagates WITHOUT
 * the finally clear; the with-expression (clarify_asker/bind_asker) and body
 * are inside try/finally, so the clear fires on every path from there on. */
/* PoP: tmp_run_plugin_command @ tui_gateway/methods_tools_plugin_commands.py:_run_plugin_command */
static json_t *tmp_run_plugin_command(tmp_ctx_t *ctx, const json_t *args) {
    const json_t *handler = json_obj_get(args, "handler");
    const json_t *arg = json_obj_get(args, "arg");
    const json_t *session = json_obj_get(args, "session");
    const char *sid = json_get_str(args, "sid", "");
    (void)sid;  /* sid rides inside the clarify_asker lambda (never invoked) */
    json_t *plugins_m = tmp_tools_mod_seam(ctx, "hermes_cli.plugins");
    if (tmp_is_raise(plugins_m)) return plugins_m;
    json_t *dispatch_m = tmp_tools_mod_seam(ctx, "hermes_cli.plugin_command_dispatch");
    if (tmp_is_raise(dispatch_m)) { json_free(plugins_m); return dispatch_m; }
    json_t *raise = NULL;
    char *key = tmp_strdup("");
    if (tmp_truthy(session)) {
        const json_t *sk = tmp_dict_get_v(session, "session_key", &raise);
        if (raise) { json_free(plugins_m); json_free(dispatch_m); return raise; }
        free(key);
        key = tmp_str_or_empty(sk);
    }
    json_t *tokens = NULL;
    if (tmp_truthy(session)) {
        const json_t *cv = tmp_dict_get_v(session, "cwd", &raise);
        if (raise) {
            free(key); json_free(plugins_m); json_free(dispatch_m); return raise;
        }
        char *cwd = tmp_str_or_empty(cv);
        json_t *sa = tmp_args0();
        tmp_asets(sa, "session_key", key);
        tmp_asets(sa, "cwd", cwd);
        free(cwd);
        tokens = tmp_call(ctx, "set_session_context", sa);
        if (tmp_is_raise(tokens)) {
            free(key); json_free(plugins_m); json_free(dispatch_m);
            return tokens;
        }
    } else {
        tokens = json_array();
    }
    json_t *pa = tmp_args0();
    tmp_aset(pa, "session", session);
    json_t *platform = tmp_call(ctx, "session_source", pa);
    if (tmp_is_raise(platform)) {
        json_free(tokens); free(key);
        json_free(plugins_m); json_free(dispatch_m);
        return platform;
    }
    json_t *session_id = NULL;
    if (tmp_truthy(session)) {
        const json_t *agent = tmp_dict_get_v(session, "agent", &raise);
        if (raise) {
            json_free(platform); json_free(tokens); free(key);
            json_free(plugins_m); json_free(dispatch_m);
            return raise;
        }
        const json_t *asid = (agent && agent->type == JSON_OBJECT)
                             ? json_obj_get(agent, "session_id") : NULL;
        session_id = tmp_truthy(asid) ? json_copy(asid) : json_string(key);
    } else {
        session_id = json_string("");
    }
    json_t *choices_m = tmp_tools_mod_seam(ctx, "hermes_cli.plugin_choices");
    if (tmp_is_raise(choices_m)) {
        json_free(session_id); json_free(platform); json_free(tokens); free(key);
        json_free(plugins_m); json_free(dispatch_m);
        return choices_m;
    }
    /* try: with choices.bind_asker(choices.clarify_asker(lambda ...)): body */
    json_t *result = NULL;   /* owned; may be a raise spec */
    json_t *aa = tmp_args0();
    tmp_asets(aa, "callback", "_clarify_block");
    json_t *asker = tmp_call(ctx, "plugin_choices.clarify_asker", aa);
    if (tmp_is_raise(asker)) {
        result = asker;
    } else {
        json_t *bad = tmp_mod_check(choices_m, "hermes_cli.plugin_choices", "bind_asker");
        if (bad) {
            json_free(asker);
            result = bad;
        } else {
            json_t *ba = tmp_args0();
            tmp_aset(ba, "asker", asker);
            json_free(asker);
            json_t *cm = tmp_call(ctx, "plugin_choices.bind_asker", ba);
            if (tmp_is_raise(cm)) {
                result = cm;   /* silent CM: no enter/exit events */
            } else {
                json_free(cm);
                bad = tmp_mod_check(dispatch_m, "hermes_cli.plugin_command_dispatch",
                                    "invoke_plugin_command");
                if (bad) {
                    result = bad;
                } else {
                    json_t *ia = tmp_args0();
                    tmp_aset(ia, "handler", handler);
                    tmp_aset(ia, "arg", arg);
                    tmp_asets(ia, "session_key", key);
                    tmp_aset(ia, "platform", platform);
                    json_set(ia, "session_id", session_id);
                    session_id = NULL;   /* adopted */
                    json_t *inv = tmp_call(ctx, "plugin_command_dispatch.invoke_plugin_command", ia);
                    if (tmp_is_raise(inv)) {
                        result = inv;
                    } else {
                        bad = tmp_mod_check(plugins_m, "hermes_cli.plugins",
                                            "resolve_plugin_command_result");
                        if (bad) {
                            json_free(inv);
                            result = bad;
                        } else {
                            json_t *ra = tmp_args0();
                            tmp_aset(ra, "result", inv);
                            json_free(inv);
                            json_t *res = tmp_call(ctx, "plugins.resolve_plugin_command_result", ra);
                            if (tmp_is_raise(res)) {
                                result = res;
                            } else {
                                char *s = tmp_str_or_empty(res);
                                json_free(res);
                                result = json_string(s);
                                free(s);
                            }
                        }
                    }
                }
            }
        }
    }
    /* finally: _clear_session_context(tokens) — its raise replaces result */
    json_t *ca = tmp_args0();
    tmp_aset(ca, "tokens", tokens);
    json_free(tokens);
    json_t *c = tmp_call(ctx, "clear_session_context", ca);
    if (tmp_is_raise(c)) {
        json_free(result);
        result = c;
    } else {
        json_free(c);
    }
    json_free(session_id);
    json_free(platform);
    free(key);
    json_free(plugins_m);
    json_free(dispatch_m);
    json_free(choices_m);
    return result;
}

/*
 * Both CMs (_session_home_scope, _session_profile_runtime_scope) are silent:
 * construction seam only. The except-Exception around scope construction +
 * __enter__ consults _is_registry_command; BaseException skips the consult. */
/* PoP: tmp_dispatch_plugin_in_profile_scope @ tui_gateway/methods_tools_plugin_commands.py:_dispatch_plugin_in_profile_scope */
static json_t *tmp_dispatch_plugin_in_profile_scope(tmp_ctx_t *ctx, const json_t *args) {
    const json_t *name = json_obj_get(args, "name");
    const json_t *arg = json_obj_get(args, "arg");
    const json_t *session = json_obj_get(args, "session");
    const char *sid = json_get_str(args, "sid", "");
    const json_t *profile = json_obj_get(args, "profile");
    json_t *ha = tmp_args0();
    tmp_aset(ha, "session", session);
    tmp_aset(ha, "profile", profile);
    json_t *hs = tmp_call(ctx, "session_home_scope", ha);
    if (tmp_is_raise(hs)) return hs;
    json_free(hs);
    /* with-body: the absent-nowait gate */
    json_t *pm = tmp_tools_mod_seam(ctx, "hermes_cli.plugins");
    if (tmp_is_raise(pm)) return pm;
    json_t *bad = tmp_mod_check(pm, "hermes_cli.plugins", "plugin_command_absent_nowait");
    json_free(pm);
    if (bad) return bad;
    json_t *na = tmp_args0();
    tmp_aset(na, "name", name);
    json_t *absent = tmp_call(ctx, "plugins.plugin_command_absent_nowait", na);
    if (tmp_is_raise(absent)) return absent;
    bool gone = tmp_truthy(absent);
    json_free(absent);
    if (gone) return NULL;
    /* try: scope = _session_profile_runtime_scope(session or {...}); enter */
    json_t *scope_raise = NULL;
    json_t *scope_sess = NULL;
    if (tmp_truthy(session)) {
        scope_sess = json_copy(session);
    } else {
        json_t *pa = tmp_args0();
        tmp_aset(pa, "profile", profile);
        json_t *ph = tmp_call(ctx, "profile_home", pa);
        if (tmp_is_raise(ph)) {
            scope_raise = ph;
        } else {
            char *s = tmp_str_or_empty(ph);
            json_free(ph);
            scope_sess = json_object();
            json_set(scope_sess, "profile_home", s[0] ? json_string(s) : json_null());
            free(s);
        }
    }
    json_t *scope = NULL;
    if (!scope_raise) {
        json_t *sa = tmp_args0();
        json_set(sa, "session", scope_sess);
        scope_sess = NULL;   /* adopted */
        scope = tmp_call(ctx, "session_profile_runtime_scope", sa);
        if (tmp_is_raise(scope)) {
            scope_raise = scope;
            scope = NULL;
        }
    }
    if (scope_raise) {
        json_free(scope_sess);
        if (tmp_is_base_exc(tmp_raise_kind(scope_raise))) return scope_raise;
        json_t *ra = tmp_args0();
        tmp_aset(ra, "name", name);
        json_t *reg = tmp_call(ctx, "is_registry_command", ra);
        if (tmp_is_raise(reg)) { json_free(scope_raise); return reg; }
        bool builtin = tmp_truthy(reg);
        json_free(reg);
        if (builtin) { json_free(scope_raise); return NULL; }
        return scope_raise;
    }
    json_free(scope);   /* silent CM: __enter__/__exit__ emit no events */
    /* try: handler lookup + run; finally: scope.__exit__(None,None,None) */
    json_t *hna = tmp_args0();
    tmp_aset(hna, "name", name);
    json_t *handler = tmp_call(ctx, "plugin_command_handler", hna);
    if (tmp_is_raise(handler)) return handler;
    if (!handler || handler->type == JSON_NULL) {
        json_free(handler);
        return NULL;
    }
    json_t *rpa = tmp_args0();
    tmp_aset(rpa, "handler", handler);
    tmp_aset(rpa, "arg", arg);
    tmp_aset(rpa, "session", session);
    tmp_asets(rpa, "sid", sid);
    json_free(handler);
    return tmp_call(ctx, "run_plugin_command", rpa);
}

/*
 * contextlib.suppress(Exception): plain kinds are swallowed (return None);
 * BaseException-only kinds pass through. */
/* PoP: tmp_dispatch_plugin @ tui_gateway/methods_tools.py:_dispatch_plugin */
static json_t *tmp_dispatch_plugin(tmp_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    const json_t *session = json_obj_get(args, "session");
    const json_t *name = json_obj_get(args, "name");
    const json_t *arg = json_obj_get(args, "arg");
    json_t *raise = NULL;
    const json_t *sid_v = tmp_params_get(params, "session_id", &raise);
    if (raise) {
        if (tmp_is_base_exc(tmp_raise_kind(raise))) return raise;
        json_free(raise);
        return NULL;
    }
    /* params.get("session_id", "") — absent defaults to "", NOT None. */
    json_t *sid_def = NULL;
    if (!sid_v) sid_v = sid_def = json_string("");
    json_t *sa = tmp_args0();
    tmp_aset(sa, "params", params);
    tmp_asets(sa, "key", "profile");
    json_t *prof = tmp_call(ctx, "str_arg", sa);
    if (tmp_is_raise(prof)) {
        json_free(sid_def);
        if (tmp_is_base_exc(tmp_raise_kind(prof))) return prof;
        json_free(prof);
        return NULL;
    }
    json_t *da = tmp_args0();
    tmp_aset(da, "name", name);
    tmp_aset(da, "arg", arg);
    tmp_aset(da, "session", session);
    tmp_aset(da, "sid", sid_v);
    json_free(sid_def);
    tmp_aset(da, "profile", tmp_truthy(prof) ? prof : NULL);
    json_free(prof);
    json_t *out = tmp_call(ctx, "dispatch_plugin_in_profile_scope", da);
    if (tmp_is_raise(out)) {
        if (tmp_is_base_exc(tmp_raise_kind(out))) return out;
        json_free(out);
        return NULL;
    }
    if (!out || out->type == JSON_NULL) {
        json_free(out);
        return NULL;
    }
    json_t *p = json_object();
    json_set_hdr(p, "type", "plugin");
    json_set(p, "output", out);
    return tmp_ok(ctx, rid, p);
}

/* PoP: tmp_refresh_plugin_sessions @ tui_gateway/methods_tools.py:refresh_plugin_sessions */
static json_t *tmp_refresh_plugin_sessions(tmp_ctx_t *ctx, const json_t *args) {
    const json_t *home = json_obj_get(args, "home");
    const char *plugin_key = json_get_str(args, "plugin_key", "");
    const json_t *note = json_obj_get(args, "note");
    json_t *a = tmp_args0();
    tmp_aset(a, "home", home);
    tmp_asetb(a, "preserve_prefix", true);
    if (tmp_truthy(note)) {
        json_t *pn = json_array();
        json_append(pn, json_string(plugin_key));
        json_append(pn, json_copy(note));
        json_set(a, "plugin_note", pn);
    }
    json_t *r = tmp_call(ctx, "refresh_live_sessions", a);
    if (tmp_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/*
 * _sessions_lock is silent; _sessions.items() is the sessions_items seam.
 * The per-session try wraps the scope construction + toolset refresh; the
 * plugin_note queue and session.info emit run AFTER the except, unguarded.
 */
/* PoP: tmp_refresh_live_sessions @ tui_gateway/methods_tools.py:_refresh_live_sessions */
static json_t *tmp_refresh_live_sessions(tmp_ctx_t *ctx, const json_t *args) {
    const json_t *home = json_obj_get(args, "home");
    bool preserve_prefix = json_get_bool(args, "preserve_prefix", false);
    const json_t *plugin_note = json_obj_get(args, "plugin_note");
    json_t *want = NULL;
    if (home && home->type != JSON_NULL) {
        json_t *wa = tmp_args0();
        tmp_aset(wa, "home", home);
        want = tmp_call(ctx, "hermes_constants.hermes_home_key", wa);
        if (tmp_is_raise(want)) return want;
    }
    json_t *items = tmp_call(ctx, "sessions_items", tmp_args0());
    if (tmp_is_raise(items)) { json_free(want); return items; }
    json_t *live = json_array();
    json_t *filter_raise = NULL;
    if (items && items->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(items); i++) {
            const json_t *pair = json_get(items, i);
            if (!pair || pair->type != JSON_ARRAY || json_len(pair) != 2) {
                filter_raise = tmp_err_spec("ValueError",
                                            "not enough values to unpack (expected 2, got 0)");
                break;
            }
            const json_t *sid = json_get(pair, 0);
            const json_t *sess = json_get(pair, 1);
            json_t *gr = NULL;
            const json_t *agent = tmp_dict_get_v(sess, "agent", &gr);
            if (gr) { filter_raise = gr; break; }
            if (!agent || agent->type == JSON_NULL) continue;
            if (want) {
                const json_t *ph = json_obj_get(sess, "profile_home");
                json_t *owned_ph = NULL;
                if (!tmp_truthy(ph)) {
                    owned_ph = tmp_call(ctx, "get_process_hermes_home", tmp_args0());
                    if (tmp_is_raise(owned_ph)) { filter_raise = owned_ph; break; }
                    ph = owned_ph;
                }
                json_t *ka = tmp_args0();
                tmp_aset(ka, "home", ph);
                json_t *hk = tmp_call(ctx, "hermes_constants.hermes_home_key", ka);
                json_free(owned_ph);
                if (tmp_is_raise(hk)) { filter_raise = hk; break; }
                bool match = tmp_json_eq(hk, want);
                json_free(hk);
                if (!match) continue;
            }
            json_t *lp = json_array();
            json_append(lp, json_copy(sid));
            json_append(lp, json_copy(sess));
            json_append(live, lp);
        }
    }
    json_free(items);
    if (filter_raise) {
        json_free(live); json_free(want);
        return filter_raise;
    }
    json_t *refresh_m = tmp_tools_mod_seam(ctx, "tools.mcp_tool_agent");
    if (tmp_is_raise(refresh_m)) { json_free(live); json_free(want); return refresh_m; }
    for (size_t i = 0; i < json_len(live); i++) {
        const json_t *pair = json_get(live, i);
        const json_t *sid = json_get(pair, 0);
        const json_t *sess = json_get(pair, 1);
        const json_t *agent = json_obj_get(sess, "agent");
        json_t *body_raise = NULL;
        json_t *enabled = NULL, *disabled = NULL;
        json_t *sca = tmp_args0();
        tmp_aset(sca, "session", sess);
        json_t *scope = tmp_call(ctx, "session_profile_runtime_scope", sca);
        if (tmp_is_raise(scope)) {
            body_raise = scope;
        } else {
            json_free(scope);   /* silent CM */
            const json_t *platform = (agent && agent->type == JSON_OBJECT)
                                     ? json_obj_get(agent, "platform") : NULL;
            json_t *ea = tmp_args0();
            tmp_aset(ea, "platform", platform);
            enabled = tmp_call(ctx, "load_enabled_toolsets", ea);
            if (tmp_is_raise(enabled)) {
                body_raise = enabled; enabled = NULL;
            } else {
                json_t *ce = tmp_args0();
                tmp_aset(ce, "enabled", enabled);
                tmp_aset(ce, "agent", agent);
                json_free(enabled);
                enabled = tmp_call(ctx, "carry_onboarding_toolset", ce);
                if (tmp_is_raise(enabled)) { body_raise = enabled; enabled = NULL; }
            }
            if (!body_raise) {
                disabled = tmp_call(ctx, "load_disabled_toolsets", tmp_args0());
                if (tmp_is_raise(disabled)) { body_raise = disabled; disabled = NULL; }
            }
            if (!body_raise) {
                json_t *bad = tmp_mod_check(refresh_m, "tools.mcp_tool_agent",
                                            "refresh_agent_mcp_tools");
                if (bad) {
                    body_raise = bad;
                } else {
                    json_t *ra = tmp_args0();
                    tmp_aset(ra, "agent", agent);
                    tmp_aset(ra, "enabled_override", enabled);
                    tmp_aset(ra, "disabled_override", disabled);
                    tmp_asetb(ra, "quiet_mode", true);
                    tmp_asetb(ra, "preserve_prefix", preserve_prefix);
                    json_t *r = tmp_call(ctx, "mcp_tool_agent.refresh_agent_mcp_tools", ra);
                    if (tmp_is_raise(r)) body_raise = r;
                    else json_free(r);
                }
            }
        }
        json_free(enabled);
        json_free(disabled);
        if (body_raise) {
            if (tmp_is_base_exc(tmp_raise_kind(body_raise))) {
                json_free(refresh_m); json_free(live); json_free(want);
                return body_raise;
            }
            char *sids = tmp_pystr(sid);
            const char *emsg = json_get_str(body_raise, "message", "");
            char msg[512];
            snprintf(msg, sizeof msg,
                     "Failed to refresh cached agent tools (session %s): %s",
                     sids, emsg);
            free(sids);
            json_free(body_raise);
            json_t *waa = tmp_args0();
            tmp_asets(waa, "message", msg);
            json_t *w = tmp_call(ctx, "logger.warning", waa);
            if (tmp_is_raise(w)) {
                json_free(refresh_m); json_free(live); json_free(want);
                return w;
            }
            json_free(w);
        }
        if (tmp_truthy(plugin_note)) {
            if (!plugin_note || plugin_note->type != JSON_ARRAY || json_len(plugin_note) != 2) {
                json_free(refresh_m); json_free(live); json_free(want);
                return tmp_err_spec("ValueError",
                                    "not enough values to unpack (expected 2, got 0)");
            }
            json_t *pnm = tmp_tools_mod_seam(ctx, "agent.plugin_live_notes");
            if (tmp_is_raise(pnm)) {
                json_free(refresh_m); json_free(live); json_free(want);
                return pnm;
            }
            json_t *bad = tmp_mod_check(pnm, "agent.plugin_live_notes", "queue_plugin_notice");
            json_free(pnm);
            if (bad) { json_free(refresh_m); json_free(live); json_free(want); return bad; }
            json_t *qa = tmp_args0();
            tmp_aset(qa, "agent", agent);
            tmp_aset(qa, "plugin_key", json_get(plugin_note, 0));
            tmp_aset(qa, "note", json_get(plugin_note, 1));
            json_t *q = tmp_call(ctx, "plugin_live_notes.queue_plugin_notice", qa);
            if (tmp_is_raise(q)) {
                json_free(refresh_m); json_free(live); json_free(want);
                return q;
            }
            json_free(q);
        }
        json_t *sia = tmp_args0();
        tmp_aset(sia, "agent", agent);
        tmp_aset(sia, "session", sess);
        json_t *si = tmp_call(ctx, "session_info", sia);
        if (tmp_is_raise(si)) {
            json_free(refresh_m); json_free(live); json_free(want);
            return si;
        }
        json_t *ea2 = tmp_args0();
        tmp_asets(ea2, "event", "session.info");
        tmp_aset(ea2, "sid", sid);
        json_set(ea2, "info", si);
        json_t *em = tmp_call(ctx, "emit", ea2);
        if (tmp_is_raise(em)) {
            json_free(refresh_m); json_free(live); json_free(want);
            return em;
        }
        json_free(em);
    }
    json_free(refresh_m);
    json_free(live);
    json_free(want);
    return NULL;
}

/* PoP: tmp_plugin_server_rows @ tui_gateway/methods_tools_plugin_servers.py:_plugin_server_rows */
static json_t *tmp_plugin_server_rows(tmp_ctx_t *ctx, const json_t *args) {
    const json_t *plugin_dir = json_obj_get(args, "plugin_dir");
    const json_t *key = json_obj_get(args, "key");
    bool portable = tmp_truthy(json_obj_get(args, "portable"));
    const json_t *catalog_titles = json_obj_get(args, "catalog_titles");
    if (!portable || !plugin_dir || plugin_dir->type == JSON_NULL)
        return json_array();
    const char *dir = tmp_str(plugin_dir);
    json_t *m = tmp_tools_mod_seam(ctx, "hermes_cli.agent_plugins");
    if (tmp_is_raise(m)) return m;
    json_t *bad = tmp_mod_check(m, "hermes_cli.agent_plugins", "load_agent_plugin");
    json_free(m);
    if (bad) return bad;
    json_t *la = tmp_args0();
    tmp_asets(la, "plugin_dir", dir);
    tmp_asets(la, "root", dir);
    json_t *pkg = tmp_call(ctx, "agent_plugins.load_agent_plugin", la);
    if (tmp_is_raise(pkg)) return pkg;
    const json_t *manifest = NULL;
    bad = tmp_ns_attr(pkg, "manifest", &manifest);
    if (bad) { json_free(pkg); return bad; }
    json_t *raise = NULL;
    const json_t *ext = tmp_dict_get_v(manifest, "extensions", &raise);
    if (raise) { json_free(pkg); return raise; }
    json_t *empty1 = json_object();
    if (!ext) ext = empty1;
    const json_t *ns2 = tmp_dict_get_v(ext, "com.nousresearch.hermes", &raise);
    if (raise) { json_free(empty1); json_free(pkg); return raise; }
    json_t *empty2 = json_object();
    if (!ns2) ns2 = empty2;
    const json_t *declared = tmp_dict_get_v(ns2, "servers", &raise);
    if (raise) { json_free(empty1); json_free(empty2); json_free(pkg); return raise; }
    json_t *empty3 = json_object();
    if (!declared) declared = empty3;
    if (declared->type != JSON_OBJECT) {
        /* isinstance(declared, dict) gate */
        json_free(empty1); json_free(empty2); json_free(empty3); json_free(pkg);
        return json_array();
    }
    json_t *m2 = tmp_tools_mod_seam(ctx, "hermes_cli.plugins_manifest");
    if (tmp_is_raise(m2)) {
        json_free(empty1); json_free(empty2); json_free(empty3); json_free(pkg);
        return m2;
    }
    json_t *m3 = tmp_tools_mod_seam(ctx, "tools.mcp_liveness");
    if (tmp_is_raise(m3)) {
        json_free(m2); json_free(empty1); json_free(empty2); json_free(empty3); json_free(pkg);
        return m3;
    }
    json_t *m4 = tmp_tools_mod_seam(ctx, "tools.mcp_tool_common");
    if (tmp_is_raise(m4)) {
        json_free(m3); json_free(m2);
        json_free(empty1); json_free(empty2); json_free(empty3); json_free(pkg);
        return m4;
    }
    json_t *m5 = tmp_tools_mod_seam(ctx, "tools.mcp_tool_scope");
    if (tmp_is_raise(m5)) {
        json_free(m4); json_free(m3); json_free(m2);
        json_free(empty1); json_free(empty2); json_free(empty3); json_free(pkg);
        return m5;
    }
    /* display_name = str(manifest.get("name") or "") or None */
    const json_t *name_v = tmp_dict_get_v(manifest, "name", &raise);
    if (raise) {
        json_free(m5); json_free(m4); json_free(m3); json_free(m2);
        json_free(empty1); json_free(empty2); json_free(empty3); json_free(pkg);
        return raise;
    }
    char *dn = tmp_str_or_empty(name_v);
    if (!dn[0]) { free(dn); dn = NULL; }
    json_t *sidecar_m = tmp_tools_mod_seam(ctx, "hermes_cli.plugins_cmd_catalog");
    if (tmp_is_raise(sidecar_m)) {
        free(dn);
        json_free(m5); json_free(m4); json_free(m3); json_free(m2);
        json_free(empty1); json_free(empty2); json_free(empty3); json_free(pkg);
        return sidecar_m;
    }
    bad = tmp_mod_check(sidecar_m, "hermes_cli.plugins_cmd_catalog", "catalog_install_record");
    json_free(sidecar_m);
    if (bad) {
        free(dn);
        json_free(m5); json_free(m4); json_free(m3); json_free(m2);
        json_free(empty1); json_free(empty2); json_free(empty3); json_free(pkg);
        return bad;
    }
    json_t *sia = tmp_args0();
    tmp_asets(sia, "plugin_dir", dir);
    json_t *sidecar = tmp_call(ctx, "plugins_cmd_catalog.catalog_install_record", sia);
    if (tmp_is_raise(sidecar)) {
        free(dn);
        json_free(m5); json_free(m4); json_free(m3); json_free(m2);
        json_free(empty1); json_free(empty2); json_free(empty3); json_free(pkg);
        return sidecar;
    }
    if (tmp_truthy(sidecar)) {
        const json_t *cn_v = tmp_dict_get_v(sidecar, "catalog_name", &raise);
        if (raise) {
            json_free(sidecar); free(dn);
            json_free(m5); json_free(m4); json_free(m3); json_free(m2);
            json_free(empty1); json_free(empty2); json_free(empty3); json_free(pkg);
            return raise;
        }
        char *cn = tmp_str_or_empty(cn_v);
        json_t *empty_titles = json_object();
        const json_t *titles = tmp_truthy(catalog_titles) ? catalog_titles : empty_titles;
        const json_t *title = tmp_dict_get_v(titles, cn, &raise);
        free(cn);
        json_free(empty_titles);
        if (raise) {
            json_free(sidecar); free(dn);
            json_free(m5); json_free(m4); json_free(m3); json_free(m2);
            json_free(empty1); json_free(empty2); json_free(empty3); json_free(pkg);
            return raise;
        }
        if (tmp_truthy(title)) {
            free(dn);
            const char *ts = tmp_str(title);
            dn = tmp_strdup(ts ? ts : "");
        }
    }
    json_free(sidecar);
    json_t *rows = json_array();
    json_t *keys = tmp_obj_keys(declared);
    json_t *skeys = tmp_sorted_strings(keys);
    json_free(keys);
    for (size_t i = 0; i < json_len(skeys); i++) {
        const char *name = tmp_str(json_get(skeys, i));
        bad = tmp_mod_check(m2, "hermes_cli.plugins_manifest", "portable_mcp_server_name");
        if (bad) goto fail;
        json_t *na = tmp_args0();
        tmp_aset(na, "key", key);
        tmp_asets(na, "server", name);
        json_t *internal = tmp_call(ctx, "plugins_manifest.portable_mcp_server_name", na);
        if (tmp_is_raise(internal)) { bad = internal; goto fail; }
        bad = tmp_mod_check(m5, "tools.mcp_tool_scope", "_resolve_server_key");
        if (bad) { json_free(internal); goto fail; }
        json_t *rka = tmp_args0();
        tmp_aset(rka, "server", internal);
        json_t *conn = tmp_call(ctx, "mcp_tool_scope._resolve_server_key", rka);
        if (tmp_is_raise(conn)) { json_free(internal); bad = conn; goto fail; }
        json_t *ga = tmp_args0();
        tmp_aset(ga, "key", conn);
        json_free(conn);
        json_t *server = tmp_call(ctx, "core._servers.get", ga);
        if (tmp_is_raise(server)) { json_free(internal); bad = server; goto fail; }
        bool connected = false;
        if (server && server->type != JSON_NULL) {
            const json_t *session_v = NULL;
            bad = tmp_ns_attr(server, "session", &session_v);
            if (bad) { json_free(server); json_free(internal); goto fail; }
            if (session_v->type != JSON_NULL) {
                connected = true;
            } else {
                json_t *ra = tmp_args0();
                tmp_aset(ra, "server", json_obj_get(server, "id"));
                json_t *recycled = tmp_call(ctx, "server._is_recycled_stdio", ra);
                if (tmp_is_raise(recycled)) {
                    json_free(server); json_free(internal); bad = recycled; goto fail;
                }
                connected = tmp_truthy(recycled);
                json_free(recycled);
            }
        }
        json_free(server);
        if (connected) {
            json_t *row = json_object();
            tmp_asets(row, "name", name);
            json_set_hdr(row, "state", "connected");
            json_set_hdr(row, "sentence", "");
            json_append(rows, row);
            json_free(internal);
            continue;
        }
        json_t *dm = tmp_tools_mod_seam(ctx, "hermes_platform.declaration");
        if (tmp_is_raise(dm)) { json_free(internal); bad = dm; goto fail; }
        bad = tmp_mod_check(dm, "hermes_platform.declaration", "lookup");
        json_free(dm);
        if (bad) { json_free(internal); goto fail; }
        json_t *dla = tmp_args0();
        tmp_aset(dla, "internal_name", internal);
        json_t *decl = tmp_call(ctx, "declaration.lookup", dla);
        if (tmp_is_raise(decl)) { json_free(internal); bad = decl; goto fail; }
        bad = tmp_mod_check(m3, "tools.mcp_liveness", "status");
        if (bad) { json_free(decl); json_free(internal); goto fail; }
        json_t *sta = tmp_args0();
        tmp_aset(sta, "internal_name", internal);
        json_free(internal);
        json_t *status = tmp_call(ctx, "mcp_liveness.status", sta);
        if (tmp_is_raise(status)) { json_free(decl); bad = status; goto fail; }
        if (!decl || decl->type == JSON_NULL || !status || status->type == JSON_NULL) {
            json_t *row = json_object();
            tmp_asets(row, "name", name);
            json_set_hdr(row, "state", "unknown");
            json_set_hdr(row, "sentence", "");
            json_append(rows, row);
            json_free(decl);
            json_free(status);
            continue;
        }
        const json_t *state_v = NULL, *avail_v = NULL;
        bad = tmp_ns_attr(status, "state", &state_v);
        if (!bad) bad = tmp_ns_attr(status, "availability", &avail_v);
        if (bad) { json_free(decl); json_free(status); goto fail; }
        bad = tmp_mod_check(m3, "tools.mcp_liveness", "describe");
        if (bad) { json_free(decl); json_free(status); goto fail; }
        json_t *dsa = tmp_args0();
        tmp_aset(dsa, "decl", decl);
        tmp_aset(dsa, "availability", avail_v);
        tmp_aset(dsa, "state", state_v);
        tmp_asets(dsa, "display_name", dn);
        json_free(decl);
        json_t *sentence = tmp_call(ctx, "mcp_liveness.describe", dsa);
        if (tmp_is_raise(sentence)) { json_free(status); bad = sentence; goto fail; }
        json_t *row = json_object();
        tmp_asets(row, "name", name);
        json_set(row, "state", json_copy(state_v));
        json_set(row, "sentence", sentence);
        json_append(rows, row);
        json_free(status);
    }
    json_free(skeys);
    free(dn);
    json_free(m5); json_free(m4); json_free(m3); json_free(m2);
    json_free(empty1); json_free(empty2); json_free(empty3);
    json_free(pkg);
    return rows;
fail:
    json_free(skeys);
    free(dn);
    json_free(m5); json_free(m4); json_free(m3); json_free(m2);
    json_free(empty1); json_free(empty2); json_free(empty3);
    json_free(pkg);
    json_free(rows);
    return bad;
}

/* PoP: tmp_portable_server_tools @ tui_gateway/methods_tools_plugin_servers.py:_portable_server_tools */
static json_t *tmp_portable_server_tools(tmp_ctx_t *ctx, const json_t *args) {
    const json_t *native = json_obj_get(args, "native");
    json_t *m1 = tmp_tools_mod_seam(ctx, "tools.mcp_tool_common");
    if (tmp_is_raise(m1)) return m1;
    json_t *m2 = tmp_tools_mod_seam(ctx, "tools.mcp_tool_scope");
    if (tmp_is_raise(m2)) { json_free(m1); return m2; }
    json_t *declared = tmp_call(ctx, "manager.get_portable_mcp_servers", tmp_args0());
    if (tmp_is_raise(declared)) { json_free(m2); json_free(m1); return declared; }
    json_t *plugins = tmp_call(ctx, "manager.get_portable_mcp_server_plugins", tmp_args0());
    if (tmp_is_raise(plugins)) {
        json_free(declared); json_free(m2); json_free(m1); return plugins;
    }
    json_t *listed = json_object();
    json_t *raise = NULL;
    if (plugins && plugins->type == JSON_OBJECT) {
        for (size_t i = 0; i < plugins->c.count; i++) {
            const char *server_name = plugins->c.keys[i];
            const json_t *owner_v = plugins->c.items[i];
            const char *owner = tmp_str(owner_v);
            if (!owner) owner = "";
            if (!json_obj_get(listed, owner))
                json_set(listed, owner, json_array());
            json_t *names = json_obj_get(listed, owner);
            const json_t *nv = native ? json_obj_get(native, server_name) : NULL;
            if (nv) {   /* server_name in native (presence, null included) */
                json_t *ia = tmp_args0();
                tmp_aset(ia, "cfg", nv);
                json_t *id1 = tmp_call(ctx, "server_identity", ia);
                if (tmp_is_raise(id1)) { raise = id1; break; }
                const json_t *dv = declared ? json_obj_get(declared, server_name) : NULL;
                json_t *ib = tmp_args0();
                tmp_aset(ib, "cfg", dv);
                json_t *id2 = tmp_call(ctx, "server_identity", ib);
                if (tmp_is_raise(id2)) { json_free(id1); raise = id2; break; }
                bool differ = !tmp_json_eq(id1, id2);
                json_free(id1);
                json_free(id2);
                if (differ) continue;
            }
            json_t *bad = tmp_mod_check(m2, "tools.mcp_tool_scope", "_resolve_server_key");
            if (bad) { raise = bad; break; }
            json_t *rka = tmp_args0();
            tmp_asets(rka, "server", server_name);
            json_t *key = tmp_call(ctx, "mcp_tool_scope._resolve_server_key", rka);
            if (tmp_is_raise(key)) { raise = key; break; }
            json_t *ga = tmp_args0();
            tmp_aset(ga, "key", key);
            json_t *server = tmp_call(ctx, "core._servers.get", ga);
            if (tmp_is_raise(server)) { json_free(key); raise = server; break; }
            const json_t *reg = (server && server->type == JSON_OBJECT)
                                ? json_obj_get(server, "_registered_tool_names") : NULL;
            if (tmp_truthy(reg)) tmp_set_update(names, reg);
            json_free(server);
            json_t *lga = tmp_args0();
            tmp_aset(lga, "key", key);
            json_free(key);
            json_t *lazy = tmp_call(ctx, "core._lazy_server_tool_names.get", lga);
            if (tmp_is_raise(lazy)) { raise = lazy; break; }
            if (tmp_truthy(lazy)) tmp_set_update(names, lazy);
            json_free(lazy);
        }
    }
    json_free(plugins);
    json_free(declared);
    json_free(m2);
    json_free(m1);
    if (raise) { json_free(listed); return raise; }
    return listed;
}

/* PoP: tmp_plugin_tool_ownership @ tui_gateway/methods_tools_plugin_servers.py:_plugin_tool_ownership */
static json_t *tmp_plugin_tool_ownership(tmp_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *pm = tmp_tools_mod_seam(ctx, "hermes_cli.plugins");
    if (tmp_is_raise(pm)) return pm;
    json_t *tm = tmp_tools_mod_seam(ctx, "toolsets");
    if (tmp_is_raise(tm)) { json_free(pm); return tm; }
    json_t *bad = tmp_mod_check(pm, "hermes_cli.plugins", "discover_plugins");
    if (bad) { json_free(tm); json_free(pm); return bad; }
    json_t *d = tmp_call(ctx, "plugins.discover_plugins", tmp_args0());
    if (tmp_is_raise(d)) { json_free(tm); json_free(pm); return d; }
    json_free(d);
    bad = tmp_mod_check(pm, "hermes_cli.plugins", "get_plugin_manager");
    if (bad) { json_free(tm); json_free(pm); return bad; }
    json_t *mgr = tmp_call(ctx, "get_plugin_manager", tmp_args0());
    if (tmp_is_raise(mgr)) { json_free(tm); json_free(pm); return mgr; }
    json_t *cm = tmp_tools_mod_seam(ctx, "hermes_cli.config");
    if (tmp_is_raise(cm)) { json_free(mgr); json_free(tm); json_free(pm); return cm; }
    bad = tmp_mod_check(cm, "hermes_cli.config", "load_config");
    json_free(cm);
    if (bad) { json_free(mgr); json_free(tm); json_free(pm); return bad; }
    json_t *cfg = tmp_call(ctx, "config.load_config", tmp_args0());
    if (tmp_is_raise(cfg)) { json_free(mgr); json_free(tm); json_free(pm); return cfg; }
    json_t *raise = NULL;
    const json_t *native = tmp_dict_get_v(cfg, "mcp_servers", &raise);
    if (raise) { json_free(cfg); json_free(mgr); json_free(tm); json_free(pm); return raise; }
    /* toolsets data attributes (io["toolsets.data"], untraced) */
    const json_t *data = tmp_module_data(ctx, "toolsets");
    const json_t *core_tools = data ? json_obj_get(data, "_HERMES_CORE_TOOLS") : NULL;
    if (!core_tools) {
        json_free(cfg); json_free(mgr); json_free(tm); json_free(pm);
        return tmp_err_spec("AttributeError",
                            "'types.SimpleNamespace' object has no attribute '_HERMES_CORE_TOOLS'");
    }
    json_t *builtin = tmp_set_norm(core_tools);
    const json_t *toolsets = data ? json_obj_get(data, "TOOLSETS") : NULL;
    if (!toolsets) {
        json_free(builtin); json_free(cfg); json_free(mgr); json_free(tm); json_free(pm);
        return tmp_err_spec("AttributeError",
                            "'types.SimpleNamespace' object has no attribute 'TOOLSETS'");
    }
    if (toolsets->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'values'",
                 tmp_type_name(toolsets));
        json_free(builtin); json_free(cfg); json_free(mgr); json_free(tm); json_free(pm);
        return tmp_err_spec("AttributeError", msg);
    }
    {
        for (size_t i = 0; i < toolsets->c.count; i++) {
            const json_t *entry = toolsets->c.items[i];
            const json_t *tools = tmp_dict_get_v(entry, "tools", &raise);
            if (raise) {
                json_free(builtin); json_free(cfg); json_free(mgr); json_free(tm); json_free(pm);
                return raise;
            }
            if (tools) tmp_set_update(builtin, tools);
        }
    }
    const json_t *plugins_d = NULL;
    bad = tmp_ns_attr(mgr, "_plugins", &plugins_d);
    if (bad) {
        json_free(builtin); json_free(cfg); json_free(mgr); json_free(tm); json_free(pm);
        return bad;
    }
    json_t *listed = json_object();
    if (plugins_d->type == JSON_OBJECT) {
        for (size_t i = 0; i < plugins_d->c.count; i++) {
            const char *key = plugins_d->c.keys[i];
            const json_t *loaded = plugins_d->c.items[i];
            const json_t *tr = NULL;
            bad = tmp_ns_attr(loaded, "tools_registered", &tr);
            if (bad) {
                json_free(listed); json_free(builtin); json_free(cfg);
                json_free(mgr); json_free(tm); json_free(pm);
                return bad;
            }
            json_t *names = tmp_set_norm(tr);
            json_t *diff = tmp_set_diff(names, builtin);
            json_free(names);
            json_set(listed, key, diff);
        }
    }
    json_t *empty_native = json_object();
    json_t *pa = tmp_args0();
    tmp_aset(pa, "manager", mgr);
    tmp_aset(pa, "native", (native && native->type == JSON_OBJECT) ? native : empty_native);
    json_t *pst = tmp_call(ctx, "portable_server_tools", pa);
    json_free(empty_native);
    if (tmp_is_raise(pst)) {
        json_free(listed); json_free(builtin); json_free(cfg);
        json_free(mgr); json_free(tm); json_free(pm);
        return pst;
    }
    if (pst && pst->type == JSON_OBJECT) {
        for (size_t i = 0; i < pst->c.count; i++) {
            const char *owner = pst->c.keys[i];
            if (!json_obj_get(listed, owner))
                json_set(listed, owner, json_array());
            tmp_set_update(json_obj_get(listed, owner), pst->c.items[i]);
        }
    }
    json_free(pst);
    /* seen[name] = count across ALL listed sets; keep only count == 1 */
    json_t *seen = json_object();
    for (size_t i = 0; i < listed->c.count; i++) {
        const json_t *names = listed->c.items[i];
        for (size_t j = 0; j < json_len(names); j++) {
            const char *nm = tmp_str(json_get(names, j));
            if (!nm) continue;
            double c = json_get_num(seen, nm, 0);
            json_set(seen, nm, json_number(c + 1));
        }
    }
    json_t *out = json_object();
    for (size_t i = 0; i < listed->c.count; i++) {
        const json_t *names = listed->c.items[i];
        json_t *kept = json_array();   /* names already sorted unique */
        for (size_t j = 0; j < json_len(names); j++) {
            const char *nm = tmp_str(json_get(names, j));
            if (nm && json_get_num(seen, nm, 0) == 1)
                json_append(kept, json_string(nm));
        }
        json_set(out, listed->c.keys[i], kept);
    }
    json_free(seen);
    json_free(listed);
    json_free(builtin);
    json_free(cfg);
    json_free(mgr);
    json_free(tm);
    json_free(pm);
    return out;
}

/*
 * Row key order follows the live dict literal; catalog_row_fields splats in
 * its own key order, then the update/pinned walrus blocks. Paths are
 * PurePosixPath on the PY side (forward slashes preserved on Windows). */
/* PoP: tmp_plugin_rows @ tui_gateway/methods_tools.py:_plugin_rows */
static json_t *tmp_plugin_rows(tmp_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *pc_m = tmp_tools_mod_seam(ctx, "hermes_cli.plugins_cmd");
    if (tmp_is_raise(pc_m)) return pc_m;
    json_t *cat_m = tmp_tools_mod_seam(ctx, "hermes_cli.plugins_cmd_catalog");
    if (tmp_is_raise(cat_m)) { json_free(pc_m); return cat_m; }
    json_t *bad = tmp_mod_check(pc_m, "hermes_cli.plugins_cmd", "_get_enabled_set");
    if (bad) { json_free(cat_m); json_free(pc_m); return bad; }
    json_t *en_raw = tmp_call(ctx, "plugins_cmd._get_enabled_set", tmp_args0());
    if (tmp_is_raise(en_raw)) { json_free(cat_m); json_free(pc_m); return en_raw; }
    json_t *enabled = tmp_set_norm(en_raw);
    json_free(en_raw);
    bad = tmp_mod_check(pc_m, "hermes_cli.plugins_cmd", "_get_disabled_set");
    if (bad) { json_free(enabled); json_free(cat_m); json_free(pc_m); return bad; }
    json_t *di_raw = tmp_call(ctx, "plugins_cmd._get_disabled_set", tmp_args0());
    if (tmp_is_raise(di_raw)) {
        json_free(enabled); json_free(cat_m); json_free(pc_m); return di_raw;
    }
    json_t *disabled = tmp_set_norm(di_raw);
    json_free(di_raw);
    bad = tmp_mod_check(cat_m, "hermes_cli.plugins_cmd_catalog", "catalog_rows_maps");
    if (bad) { json_free(disabled); json_free(enabled); json_free(cat_m); json_free(pc_m); return bad; }
    json_t *maps = tmp_call(ctx, "plugins_cmd_catalog.catalog_rows_maps", tmp_args0());
    if (tmp_is_raise(maps)) {
        json_free(disabled); json_free(enabled); json_free(cat_m); json_free(pc_m);
        return maps;
    }
    if (!maps || maps->type != JSON_ARRAY || json_len(maps) != 3) {
        size_t got = (maps && maps->type == JSON_ARRAY) ? json_len(maps) : 0;
        char msg[128];
        if (got < 3)
            snprintf(msg, sizeof msg,
                     "not enough values to unpack (expected 3, got %zu)", got);
        else
            snprintf(msg, sizeof msg, "too many values to unpack (expected 3)");
        json_free(maps); json_free(disabled); json_free(enabled);
        json_free(cat_m); json_free(pc_m);
        return tmp_err_spec("ValueError", msg);
    }
    json_t *pins = json_copy(json_get(maps, 0));
    json_t *versions = json_copy(json_get(maps, 1));
    json_t *titles = json_copy(json_get(maps, 2));
    json_free(maps);
    bad = tmp_mod_check(pc_m, "hermes_cli.plugins_cmd", "_read_install_metadata");
    if (bad) {
        json_free(titles); json_free(versions); json_free(pins);
        json_free(disabled); json_free(enabled); json_free(cat_m); json_free(pc_m);
        return bad;
    }
    json_t *ref_pins = tmp_call(ctx, "plugins_cmd._read_install_metadata", tmp_args0());
    if (tmp_is_raise(ref_pins)) {
        json_free(titles); json_free(versions); json_free(pins);
        json_free(disabled); json_free(enabled); json_free(cat_m); json_free(pc_m);
        return ref_pins;
    }
    json_t *cad_m = tmp_tools_mod_seam(ctx, "hermes_cli.plugins_cadence");
    if (tmp_is_raise(cad_m)) {
        json_free(ref_pins); json_free(titles); json_free(versions); json_free(pins);
        json_free(disabled); json_free(enabled); json_free(cat_m); json_free(pc_m);
        return cad_m;
    }
    bad = tmp_mod_check(cad_m, "hermes_cli.plugins_cadence", "cached_git_updates");
    json_free(cad_m);
    if (bad) {
        json_free(ref_pins); json_free(titles); json_free(versions); json_free(pins);
        json_free(disabled); json_free(enabled); json_free(cat_m); json_free(pc_m);
        return bad;
    }
    json_t *gua = tmp_args0();
    tmp_aset(gua, "ref_pins", ref_pins);
    json_t *git_updates = tmp_call(ctx, "plugins_cadence.cached_git_updates", gua);
    if (tmp_is_raise(git_updates)) {
        json_free(ref_pins); json_free(titles); json_free(versions); json_free(pins);
        json_free(disabled); json_free(enabled); json_free(cat_m); json_free(pc_m);
        return git_updates;
    }
    bad = tmp_mod_check(pc_m, "hermes_cli.plugins_cmd", "_category_active_names");
    if (bad) {
        json_free(git_updates); json_free(ref_pins); json_free(titles);
        json_free(versions); json_free(pins);
        json_free(disabled); json_free(enabled); json_free(cat_m); json_free(pc_m);
        return bad;
    }
    json_t *ac_raw = tmp_call(ctx, "plugins_cmd._category_active_names", tmp_args0());
    if (tmp_is_raise(ac_raw)) {
        json_free(git_updates); json_free(ref_pins); json_free(titles);
        json_free(versions); json_free(pins);
        json_free(disabled); json_free(enabled); json_free(cat_m); json_free(pc_m);
        return ac_raw;
    }
    json_t *active = tmp_set_norm(ac_raw);
    json_free(ac_raw);
    json_t *owned = tmp_call(ctx, "plugin_tool_ownership", tmp_args0());
    if (tmp_is_raise(owned)) {
        json_free(active); json_free(git_updates); json_free(ref_pins);
        json_free(titles); json_free(versions); json_free(pins);
        json_free(disabled); json_free(enabled); json_free(cat_m); json_free(pc_m);
        return owned;
    }
    bad = tmp_mod_check(pc_m, "hermes_cli.plugins_cmd", "_discover_all_plugins");
    if (bad) {
        json_free(owned); json_free(active); json_free(git_updates); json_free(ref_pins);
        json_free(titles); json_free(versions); json_free(pins);
        json_free(disabled); json_free(enabled); json_free(cat_m); json_free(pc_m);
        return bad;
    }
    json_t *disc = tmp_call(ctx, "plugins_cmd._discover_all_plugins", tmp_args0());
    if (tmp_is_raise(disc)) {
        json_free(owned); json_free(active); json_free(git_updates); json_free(ref_pins);
        json_free(titles); json_free(versions); json_free(pins);
        json_free(disabled); json_free(enabled); json_free(cat_m); json_free(pc_m);
        return disc;
    }
    json_t *sorted_disc = tmp_sorted_tuples(disc);
    json_free(disc);
    json_t *rows = json_array();
    json_t *row_raise = NULL;
    for (size_t i = 0; i < json_len(sorted_disc); i++) {
        const json_t *tup = json_get(sorted_disc, i);
        if (!tup || tup->type != JSON_ARRAY || json_len(tup) != 6) {
            row_raise = tmp_err_spec("ValueError",
                                     "not enough values to unpack (expected 6, got 0)");
            break;
        }
        const json_t *name = json_get(tup, 0);
        const json_t *version = json_get(tup, 1);
        const json_t *desc = json_get(tup, 2);
        const json_t *source = json_get(tup, 3);
        const json_t *dir_v = json_get(tup, 4);
        const json_t *key = json_get(tup, 5);
        bad = tmp_mod_check(pc_m, "hermes_cli.plugins_cmd", "_plugin_status");
        if (bad) { row_raise = bad; break; }
        json_t *sta = tmp_args0();
        tmp_aset(sta, "name", name);
        tmp_aset(sta, "enabled", enabled);
        tmp_aset(sta, "disabled", disabled);
        tmp_aset(sta, "key", key);
        tmp_aset(sta, "source", source);
        tmp_aset(sta, "dir_path", dir_v);
        tmp_aset(sta, "active", active);
        json_t *status = tmp_call(ctx, "plugins_cmd._plugin_status", sta);
        if (tmp_is_raise(status)) { row_raise = status; break; }
        /* _dir_path = Path(str(_dir)) if _dir else None */
        char *norm = NULL;
        if (tmp_truthy(dir_v)) {
            char *s = tmp_pystr(dir_v);
            norm = tmp_path_norm(s);
            free(s);
        }
        bad = tmp_mod_check(pc_m, "hermes_cli.plugins_cmd", "_is_portable_plugin_dir");
        if (bad) { json_free(status); free(norm); row_raise = bad; break; }
        json_t *poa = tmp_args0();
        tmp_aset(poa, "plugin_dir", dir_v);
        json_t *portable = tmp_call(ctx, "plugins_cmd._is_portable_plugin_dir", poa);
        if (tmp_is_raise(portable)) {
            json_free(status); free(norm); row_raise = portable; break;
        }
        json_t *row = json_object();
        tmp_aset(row, "name", name);
        tmp_aset(row, "key", key);
        char *vs = tmp_str_or_empty(version);
        json_set(row, "version", json_string(vs));
        free(vs);
        tmp_aset(row, "description", tmp_truthy(desc) ? desc : NULL);
        if (!tmp_truthy(desc)) json_set(row, "description", json_string(""));
        tmp_aset(row, "source", source);
        json_set(row, "status", status);
        tmp_aset(row, "portable", portable);
        json_set(row, "install_dir", json_string(norm ? norm : ""));
        /* has_desktop_half: bool(_dir_path and desktop_source(_dir_path)) */
        bool hd = false;
        if (norm) {
            json_t *dh_m = tmp_tools_mod_seam(ctx, "hermes_cli.desktop_plugin_halves");
            if (tmp_is_raise(dh_m)) {
                json_free(row); json_free(portable); free(norm);
                row_raise = dh_m; break;
            }
            bad = tmp_mod_check(dh_m, "hermes_cli.desktop_plugin_halves", "desktop_source");
            json_free(dh_m);
            if (bad) {
                json_free(row); json_free(portable); free(norm);
                row_raise = bad; break;
            }
            json_t *dsa = tmp_args0();
            tmp_asets(dsa, "plugin_dir", norm);
            json_t *ds = tmp_call(ctx, "desktop_plugin_halves.desktop_source", dsa);
            if (tmp_is_raise(ds)) {
                json_free(row); json_free(portable); free(norm);
                row_raise = ds; break;
            }
            hd = tmp_truthy(ds);
            json_free(ds);
        }
        tmp_asetb(row, "has_desktop_half", hd);
        /* settings_schema */
        json_t *ps_m = tmp_tools_mod_seam(ctx, "hermes_cli.plugins_settings");
        if (tmp_is_raise(ps_m)) {
            json_free(row); json_free(portable); free(norm);
            row_raise = ps_m; break;
        }
        bad = tmp_mod_check(ps_m, "hermes_cli.plugins_settings", "plugin_settings_fields");
        json_free(ps_m);
        if (bad) {
            json_free(row); json_free(portable); free(norm);
            row_raise = bad; break;
        }
        json_t *sfa = tmp_args0();
        tmp_aset(sfa, "key", key);
        tmp_asets(sfa, "plugin_dir", norm);
        json_t *sf = tmp_call(ctx, "plugins_settings.plugin_settings_fields", sfa);
        if (tmp_is_raise(sf)) {
            json_free(row); json_free(portable); free(norm);
            row_raise = sf; break;
        }
        json_set(row, "settings_schema", sf);
        /* servers: _plugin_server_rows(_dir_path, key, portable=..., catalog_titles=titles) */
        json_t *sra = tmp_args0();
        tmp_asets(sra, "plugin_dir", norm);
        tmp_aset(sra, "key", key);
        tmp_aset(sra, "portable", portable);
        tmp_aset(sra, "catalog_titles", titles);
        json_t *servers = tmp_call(ctx, "plugin_server_rows", sra);
        if (tmp_is_raise(servers)) {
            json_free(row); json_free(portable); free(norm);
            row_raise = servers; break;
        }
        json_set(row, "servers", servers);
        /* tools: owned_tools.get(key, []) */
        const json_t *ot = (owned && owned->type == JSON_OBJECT)
                           ? json_obj_get(owned, tmp_str(key) ? tmp_str(key) : "") : NULL;
        json_set(row, "tools", ot ? json_copy(ot) : json_array());
        /* **cat.catalog_row_fields(_dir, pins, versions) */
        bad = tmp_mod_check(cat_m, "hermes_cli.plugins_cmd_catalog", "catalog_row_fields");
        if (bad) {
            json_free(row); json_free(portable); free(norm);
            row_raise = bad; break;
        }
        json_t *cfa = tmp_args0();
        tmp_aset(cfa, "dir", dir_v);
        tmp_aset(cfa, "pins", pins);
        tmp_aset(cfa, "versions", versions);
        json_t *crf = tmp_call(ctx, "plugins_cmd_catalog.catalog_row_fields", cfa);
        if (tmp_is_raise(crf)) {
            json_free(row); json_free(portable); free(norm);
            row_raise = crf; break;
        }
        if (!crf || crf->type != JSON_OBJECT) {
            char msg[128];
            snprintf(msg, sizeof msg, "argument of type '%s' is not a mapping",
                     tmp_type_name(crf));
            json_free(crf); json_free(row); json_free(portable); free(norm);
            row_raise = tmp_err_spec("TypeError", msg);
            break;
        }
        for (size_t k = 0; k < crf->c.count; k++)
            json_set(row, crf->c.keys[k], json_copy(crf->c.items[k]));
        json_free(crf);
        /* update walrus: only when _dir_path; git_updates.get(name) */
        if (norm) {
            json_t *gr = NULL;
            char *pname = tmp_path_name(norm);
            const json_t *target = tmp_dict_get_v(git_updates, pname, &gr);
            free(pname);
            if (gr) {
                json_free(row); json_free(portable); free(norm);
                row_raise = gr; break;
            }
            if (tmp_truthy(target)) {
                tmp_asetb(row, "update_available", true);
                tmp_aset(row, "update_target", target);
            }
        }
        /* pinned walrus: pinned_revision(name, ref_pins) always fires */
        bad = tmp_mod_check(pc_m, "hermes_cli.plugins_cmd", "pinned_revision");
        if (bad) {
            json_free(row); json_free(portable); free(norm);
            row_raise = bad; break;
        }
        json_t *pra = tmp_args0();
        tmp_aset(pra, "name", name);
        tmp_aset(pra, "ref_pins", ref_pins);
        json_t *sha = tmp_call(ctx, "plugins_cmd.pinned_revision", pra);
        if (tmp_is_raise(sha)) {
            json_free(row); json_free(portable); free(norm);
            row_raise = sha; break;
        }
        if (tmp_truthy(sha)) {
            json_set(row, "pinned_sha", sha);
        } else {
            json_free(sha);
        }
        json_free(portable);
        free(norm);
        json_append(rows, row);
    }
    json_free(sorted_disc);
    json_free(owned);
    json_free(active);
    json_free(git_updates);
    json_free(ref_pins);
    json_free(titles);
    json_free(versions);
    json_free(pins);
    json_free(disabled);
    json_free(enabled);
    json_free(cat_m);
    json_free(pc_m);
    if (row_raise) { json_free(rows); return row_raise; }
    return rows;
}
