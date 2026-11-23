/*
 * tui_methods_flow.c — FLOW + leaf port of tui_gateway/methods_profiles.py
 * (hermes-agent): the profile JSON-RPC helper surface — parameter leaves,
 * best-effort wrappers, late-bound imports, profile resolution, profile.yaml
 * reads, roster session summaries, the ui_meta CAS writer and the model-pin
 * confirm handshake.
 *
 * NOTE: the quarry has no tui_gateway/methods.py — the TUI method surface is
 * the methods_*.py family. This port claims the fully UNCLAIMED
 * methods_profiles.py helpers (PoP annotations name the defining module).
 *
 * Ports the DECISION SEQUENCES — branch order, error-code/message composition,
 * seam call order, caller-dict mutation history and exception paths — not just
 * leaf outputs. Every live external call enters through ONE universal seam:
 * tm_call(ctx, name, args). The harness wires it to a recording stub fed the
 * case's scripted results (in.io); the oracle installs recording stubs
 * emitting the identical event shape {"name","args","ret"} from the SAME
 * scripts.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and tm_-prefixed.
 *
 * Server globals are seams: methods_profiles bodies are rebound onto
 * server.py's globals at install time (method_ctx.bind_module), so _err /
 * _launch_home / _persistence_for / _profile_ui_meta_lock / _denied_source /
 * is_truthy_value are bare free variables. _err is seamed ("err");
 * _denied_source is CLAIMED by methods_session.py and seamed ("denied_source"),
 * never re-ported. _profile_ui_meta_lock is a silent context manager (no
 * events). is_truthy_value (utils/env.py) is a PURE leaf: transcribed below
 * (tm_is_truthy_value), run real in the oracle.
 *
 * Ported siblings compose LIVE where they are leaf-flows (_try, _best_effort,
 * _lazy, _pin_profile_model, _hermes_home_scope, _read_profile_yaml,
 * _clean_revisions, _bots_title, _clean_names, _model_provider_params,
 * _latest_message_preview, _display_count_field, _mcp_toggle_state) — their
 * seams fire from inside the composer. _resurrect_recoverable_canonical is a
 * ported sibling SEAMED in _canonical_session_row ("resurrect_recoverable_
 * canonical"); each is exercised by its own cases.
 * _profile_group_selection_context is an UNPORTED sibling, seamed
 * ("profile_group_selection_context").
 *
 * Seam name -> live call (args are informational projections):
 *   err                         _err(rid, code, msg)  [server global; no data]
 *   import_attr                 _lazy(module, name) -> getattr(__import__(
 *                               module, fromlist=[name]), name)
 *   write_profile_model         hermes_cli.profile_provision.write_profile_model(
 *                               profile_dir, provider, model, base_url=...)
 *   launch_home                 _launch_home()  [server global; Path -> posix]
 *   profile_config_scope        tui_gateway.launch_profile_policy.
 *                               profile_config_scope(path, launch_home=...)
 *                               -> context-manager marker (dict); null raises
 *                               TypeError at the live `with` site
 *   get_profile_dir             hermes_cli.profiles.get_profile_dir(name)
 *   is_dir                      Path(path).is_dir()
 *   is_file                     Path(path).is_file()
 *   read_text                   Path(path).read_text(encoding="utf-8")
 *   yaml_safe_load              hermes_yaml.safe_load(text)
 *   latest_dialogue_message     persistence.messages.latest_dialogue_message(sid)
 *   display_count               persistence.messages.display_count(str(sid))
 *   persistence_for             _persistence_for(db)  [server global; CM handle]
 *   mcp_server_enabled          tools.mcp_tool_common.mcp_server_enabled(entry)
 *   save_platform_tools         hermes_cli.tools_config._save_platform_tools(
 *                               cfg, "cli", wanted)  [wanted SET -> sorted list]
 *   save_config                 save_config(cfg)  [callback arg]
 *   write_mcp_server_enabled    tools.mcp_tool_common.write_mcp_server_enabled(
 *                               entry, enabled)
 *   unlink                      Path(path).unlink()
 *   read_user_config_raw        hermes_cli.config.read_user_config_raw()
 *   denied_source               _denied_source({"source": ...})  [claimed sibling]
 *   resurrect_recoverable_canonical  _resurrect_recoverable_canonical(
 *                               persistence, profile_path, session_id)
 *                               [ported sibling; persistence handle omitted]
 *   get_by_title                persistence.sessions.get_by_title("Bot Chat")
 *   sessions_get                persistence.sessions.get(session_id)
 *   lineage_tip                 persistence.lineage.tip(session_id)
 *   profile_scope               persistence_assembly.profile_scope(home=Path(p))
 *   open_persistence            persistence_assembly.open_persistence(scope)
 *                               -> writer handle (dict marker)
 *   unarchive_recoverable       writer.sessions.unarchive_recoverable(sid)
 *   writer_close                writer.close()  [via _best_effort]
 *   profile_group_selection_context  _profile_group_selection_context(
 *                               profile_dir, target_model)  [unported sibling]
 *   combined_selection_warning  warn(model, provider=..., selection_context=...)
 *                               [warn = _lazy("hermes_cli.model_selection_guards",
 *                               "combined_selection_warning")]
 *   atomic_yaml_write           utils.atomic_yaml_write(path, doc, sort_keys=False)
 *   logger_info                 logging.getLogger(__name__).info(fmt, *args)
 *                               -> {"msg": fmt % args}  (eager % formatting)
 *   try_fn                      the fn() callback inside _try
 *   effort_fn                   the fn() callback inside _best_effort
 *
 * Representation contract (shared with tests/oracle_tui_methods_flow.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)}; BaseException-only
 *   kinds (SystemExit, KeyboardInterrupt, GeneratorExit, CancelledError) PASS
 *   THROUGH every except-Exception site.
 * - STORE_FAILURES (persistence.core.errors) is the fixed kind set
 *   {TransientStoreError, PermanentStoreError, StoreUnavailable, StoreReplaced};
 *   _display_count_field catches exactly those names (subclass raises outside
 *   the fixture domain).
 * - RECOVERABLE_END_REASONS = {agent_close, ws_orphan_reap,
 *   superseded_by_resume, startup_orphan_reap}.
 * - Persistence/row/writer doubles: scripted dicts become SimpleNamespaces;
 *   a double is ALWAYS truthy (even {}), modeled by tm_double_truthy (object
 *   presence), NOT container truthiness. Null handles raise the live
 *   NoneType attribute/CM messages at the access site. Row scripts are dicts
 *   or null; direct attribute reads on a missing key raise
 *   "'types.SimpleNamespace' object has no attribute '<key>'".
 * - Tuples project to lists; sets to sorted lists; integral floats to ints;
 *   Paths to POSIX strings. Fixture paths are absolute POSIX, no trailing
 *   slash, no "." components (path join is string concat; .name is the last
 *   component). Fixture numbers are ints (non-integral floats are excluded
 *   from compared positions; type-gated leaves never see float literals).
 * - str()/strip()/lower()/split() fold the ASCII fixture corpus only; the
 *   strip/split whitespace set includes \x1c-\x1f (Python White_Space).
 *   str() sites see str/int/bool/null only.
 * - json.dumps(incoming) length (_configure_ui_meta cap) is transcribed with
 *   Python's default separators (', ', ': ') and ensure_ascii escaping;
 *   ui_meta values are str/int/bool/null/list/dict (no floats).
 * - State: ctx.state mirrors the caller dicts "cfg" (_save_toolset_pin /
 *   _save_mcp_toggles) and "applied" (_configure_model / _configure_ui_meta),
 *   materialized at entry and mutated in place. Other functions leave state
 *   alone.
 * - _save_mcp_toggles' `for srv in wanted` set loop: fixtures only copy from
 *   launch_mcp when wanted has ONE member (set iteration order is otherwise
 *   unobservable — no seam fires for already-present servers).
 *
 * Ownership: tm_call ADOPTS args into the trace event; its return is a fresh
 * owned node. json_set ADOPTS its value and frees the replaced node — borrowed
 * pointers to a replaced key are copied first, and a node is never json_set
 * onto its own parent key (pointer-identity guards). Seam results are
 * caller-freed.
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
typedef struct tm_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} tm_ctx_t;

/* ── basic helpers ── */
static bool tm_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static const char *tm_raise_kind(const json_t *v) {
    return tm_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}

/* BaseException-only kinds pass through every except-Exception site. */
static bool tm_is_base_exc(const char *kind) {
    return kind && (!strcmp(kind, "SystemExit") || !strcmp(kind, "KeyboardInterrupt")
                    || !strcmp(kind, "GeneratorExit") || !strcmp(kind, "CancelledError"));
}

/* STORE_FAILURES = (TransientStoreError, PermanentStoreError,
 *                   StoreUnavailable, StoreReplaced) */
static bool tm_is_store_failure(const char *kind) {
    return kind && (!strcmp(kind, "TransientStoreError")
                    || !strcmp(kind, "PermanentStoreError")
                    || !strcmp(kind, "StoreUnavailable")
                    || !strcmp(kind, "StoreReplaced"));
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool tm_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

/* A SimpleNamespace double is ALWAYS truthy (even {}); raw non-dict script
 * values keep their own truthiness. */
static bool tm_double_truthy(const json_t *v) {
    if (v && v->type == JSON_OBJECT) return true;
    return tm_truthy(v);
}

static const char *tm_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static json_t *tm_err_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* Python type name for a JSON value (fixture scalar/container domain). */
static const char *tm_type_name(const json_t *v) {
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

static char *tm_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

/* Python str() for scalars (containers are outside the str()-projection
 * fixture domain). */
static char *tm_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return tm_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return tm_strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return tm_strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && d > -9.2e18 && d < 9.2e18)
                snprintf(buf, sizeof buf, "%lld", (long long)d);
            else
                snprintf(buf, sizeof buf, "%.17g", d);
            return tm_strdup(buf);
        }
        default: return json_serialize(v);
    }
}

/* Python repr() for the logger %r positions (str / None; ASCII corpus). */
static char *tm_repr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return tm_strdup("None");
    if (v->type != JSON_STRING) return tm_pystr(v);
    const char *s = v->str_val ? v->str_val : "";
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t n = strlen(s);
    char *o = malloc(n * 4 + 3);
    if (!o) return NULL;
    size_t k = 0;
    o[k++] = q;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == (unsigned char)q || c == '\\') { o[k++] = '\\'; o[k++] = (char)c; }
        else if (c == '\n') { o[k++] = '\\'; o[k++] = 'n'; }
        else if (c == '\r') { o[k++] = '\\'; o[k++] = 'r'; }
        else if (c == '\t') { o[k++] = '\\'; o[k++] = 't'; }
        else if (c == '\a') { o[k++] = '\\'; o[k++] = 'a'; }
        else if (c == '\b') { o[k++] = '\\'; o[k++] = 'b'; }
        else if (c == '\f') { o[k++] = '\\'; o[k++] = 'f'; }
        else if (c < 0x20 || c == 0x7f) {
            k += (size_t)snprintf(o + k, 5, "\\x%02x", c);
        } else o[k++] = (char)c;
    }
    o[k++] = q;
    o[k] = '\0';
    return o;
}

/* Python str.strip() whitespace: ASCII White_Space INCLUDING \x1c-\x1f. */
static bool tm_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f'
        || c == '\v' || (c >= 0x1c && c <= 0x1f);
}

static char *tm_strip(const char *s) {
    if (!s) s = "";
    while (*s && tm_is_ws((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && tm_is_ws((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - s);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

static char *tm_lower(const char *s) {
    char *o = tm_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}

/* " ".join(s.split()) — collapse ASCII White_Space runs to single spaces. */
static char *tm_split_join(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    size_t k = 0;
    bool pending = false;
    for (size_t i = 0; i < n; i++) {
        if (tm_is_ws((unsigned char)s[i])) {
            if (k > 0) pending = true;
        } else {
            if (pending) { o[k++] = ' '; pending = false; }
            o[k++] = s[i];
        }
    }
    o[k] = '\0';
    return o;
}

/* str(params.get(k) or "").strip() — always a string result. */
static char *tm_str_strip_param(const json_t *params, const char *k) {
    const json_t *v = json_obj_get(params, k);
    char *s = tm_truthy(v) ? tm_pystr(v) : tm_strdup("");
    char *out = tm_strip(s);
    free(s);
    return out;
}

/* Deep scalar equality (Python == on the fixture domain; bool==int cross). */
static bool tm_json_eq(const json_t *a, const json_t *b) {
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
            if (!tm_json_eq(a->c.items[i], b->c.items[i])) return false;
        return true;
    }
    if (a->type == JSON_OBJECT) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !tm_json_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}

/* len(json.dumps(v)) — Python default separators (', ', ': '), ensure_ascii.
 * Fixture domain: str/int/bool/null/list/dict (no floats). */
static size_t tm_dumps_len(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 4; /* null */
    switch (v->type) {
        case JSON_BOOL: return v->bool_val ? 4 : 5;
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && d > -9.2e18 && d < 9.2e18)
                return (size_t)snprintf(buf, sizeof buf, "%lld", (long long)d);
            return (size_t)snprintf(buf, sizeof buf, "%.17g", d);
        }
        case JSON_STRING: {
            const char *s = v->str_val ? v->str_val : "";
            size_t n = 2;
            for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
                unsigned char c = *p;
                if (c == '"' || c == '\\' || c == '\n' || c == '\r'
                    || c == '\t' || c == '\b' || c == '\f') n += 2;
                else if (c < 0x20) n += 6;
                else n += 1;
            }
            return n;
        }
        case JSON_ARRAY: {
            size_t n = 2;
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) n += 2;
                n += tm_dumps_len(v->c.items[i]);
            }
            return n;
        }
        case JSON_OBJECT: {
            size_t n = 2;
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) n += 2;
                n += tm_dumps_len(json_string(v->c.keys[i]));
                n += 2; /* ": " */
                n += tm_dumps_len(v->c.items[i]);
            }
            return n;
        }
        default: return 4;
    }
}

/* POSIX path join / basename (fixture domain: absolute, no trailing slash,
 * no "." components; Path("") == Path(".") handled for completeness). */
static char *tm_path_join(const char *dir, const char *leaf) {
    if (!dir) dir = "";
    if (!leaf) leaf = "";
    if (!dir[0] || !strcmp(dir, ".")) return tm_strdup(leaf);
    size_t n = strlen(dir) + 1 + strlen(leaf);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    snprintf(o, n + 1, "%s/%s", dir, leaf);
    return o;
}

static char *tm_path_name(const char *dir) {
    if (!dir || !dir[0]) return tm_strdup("");
    const char *slash = strrchr(dir, '/');
    return tm_strdup(slash ? slash + 1 : dir);
}

/* ── the one universal seam (fail-closed) ── */
static json_t *tm_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return tm_err_spec(type, msg);
}

static json_t *tm_call(tm_ctx_t *ctx, const char *name, json_t *args) {
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
        ret = tm_err_spec("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = tm_err_spec("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = tm_make_exc(raise);
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
static json_t *tm_args0(void) { return json_object(); }
static void tm_aset(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static void tm_asets(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void tm_asetb(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void tm_asetn(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }

/* _err(rid, code, msg) — the server-global envelope seam (no data arg). */
static json_t *tm_err(tm_ctx_t *ctx, const json_t *rid, int code, const char *msg) {
    json_t *a = tm_args0();
    tm_aset(a, "rid", rid);
    tm_asetn(a, "code", (double)code);
    tm_asets(a, "message", msg);
    return tm_call(ctx, "err", a);
}

/* ── caller-dict state mirrors ── */
static json_t *tm_state_key(tm_ctx_t *ctx, const char *key) {
    if (!json_has(ctx->state, key)) json_set(ctx->state, key, json_object());
    return json_obj_get(ctx->state, key);
}

/* ── attribute access on SimpleNamespace doubles ── */
/* Returns a BORROWED value (or the borrowed default when has_default and the
 * key is absent); a missing required attribute sets *raise to the live
 * AttributeError spec. */
static const json_t *tm_row_attr(const json_t *row, const char *key,
                                 bool has_default, const json_t *def,
                                 json_t **raise) {
    *raise = NULL;
    if (row && row->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(row, key);
        if (v) return v;
        if (has_default) return def;
        char msg[192];
        snprintf(msg, sizeof msg,
                 "'types.SimpleNamespace' object has no attribute '%s'", key);
        *raise = tm_err_spec("AttributeError", msg);
        return NULL;
    }
    if (has_default) return def;
    char msg[192];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'",
             tm_type_name(row), key);
    *raise = tm_err_spec("AttributeError", msg);
    return NULL;
}

/* First sub-object access on a persistence handle: a non-dict handle raises
 * the live NoneType/str/... attribute message naming the sub-object. */
static json_t *tm_persist_check(const json_t *p, const char *sub) {
    if (p && p->type == JSON_OBJECT) return NULL;
    char msg[192];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'",
             tm_type_name(p), sub);
    return tm_err_spec("AttributeError", msg);
}

/* `with <null>:` — the py3.14 context-manager protocol TypeError. */
static json_t *tm_cm_type_error(const json_t *v) {
    char msg[192];
    snprintf(msg, sizeof msg,
             "'%s' object does not support the context manager protocol "
             "(missed __exit__ method)", tm_type_name(v));
    return tm_err_spec("TypeError", msg);
}

/* ── sorted string set (_clean_names) ── */
static int tm_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* {str(v).strip() for v in values if str(v).strip()} -> SORTED list. */
static json_t *tm_clean_names_v(const json_t *values) {
    json_t *out = json_array();
    if (!values || values->type != JSON_ARRAY) return out;
    size_t n = json_len(values);
    char **tmp = malloc((n + 1) * sizeof(char *));
    if (!tmp) return out;
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        char *s = tm_pystr(json_get(values, i));
        char *t = tm_strip(s);
        free(s);
        if (t && t[0]) tmp[k++] = t;
        else free(t);
    }
    qsort(tmp, k, sizeof(char *), tm_cmp_str);
    for (size_t i = 0; i < k; i++) {
        if (i == 0 || strcmp(tmp[i], tmp[i - 1]) != 0)
            json_append(out, json_string(tmp[i]));
        free(tmp[i]);
    }
    free(tmp);
    return out;
}

static bool tm_names_contains(const json_t *names, const char *s) {
    for (size_t i = 0; i < json_len(names); i++) {
        const char *v = tm_str(json_get(names, i));
        if (v && !strcmp(v, s)) return true;
    }
    return false;
}

/* ── is_truthy_value (utils/env.py — pure leaf, run real in the oracle) ── */
static bool tm_is_truthy_value(const json_t *value, bool def) {
    if (!value || value->type == JSON_NULL) return def;
    if (value->type == JSON_STRING) {
        char *s = tm_strip(value->str_val ? value->str_val : "");
        char *low = tm_lower(s);
        free(s);
        bool hit = low && (!strcmp(low, "1") || !strcmp(low, "true")
                           || !strcmp(low, "yes") || !strcmp(low, "on"));
        free(low);
        return hit;
    }
    return tm_truthy(value);
}

/* ── pure leaves (run REAL on both sides; no seams) ── */

/* PoP: tm_model_provider_params @ tui_gateway/methods_profiles.py:_model_provider_params */
static json_t *tm_model_provider_params(tm_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *params = json_obj_get(args, "params");
    char *model = tm_str_strip_param(params, "model");
    char *provider = tm_str_strip_param(params, "provider");
    json_t *out = json_array();
    json_append(out, json_string(model ? model : ""));
    json_append(out, json_string(provider ? provider : ""));
    free(model);
    free(provider);
    return out;
}

/* {str(k): max(0, int(v)) for k, v in raw.items()
 *  if isinstance(v, int) and not isinstance(v, bool)} */
static json_t *tm_clean_revisions_v(const json_t *raw) {
    json_t *out = json_object();
    if (!raw || raw->type != JSON_OBJECT) return out;
    for (size_t i = 0; i < raw->c.count; i++) {
        const json_t *v = raw->c.items[i];
        if (!v || v->type != JSON_NUMBER) continue;
        double d = v->num_val;
        if (d != (double)(long long)d || d < -9.2e18 || d > 9.2e18) continue;
        long long iv = (long long)d;
        json_set(out, raw->c.keys[i], json_number((double)(iv < 0 ? 0 : iv)));
    }
    return out;
}

/* PoP: tm_clean_revisions @ tui_gateway/methods_profiles.py:_clean_revisions */
static json_t *tm_clean_revisions(tm_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return tm_clean_revisions_v(json_obj_get(args, "raw"));
}

/* PoP: tm_clean_names @ tui_gateway/methods_profiles.py:_clean_names */
static json_t *tm_clean_names(tm_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return tm_clean_names_v(json_obj_get(args, "values"));
}

/* bots.get("title") if isinstance(bots, dict) else None */
static const json_t *tm_bots_title_v(const json_t *ui_meta, json_t **raise) {
    *raise = NULL;
    if (!ui_meta || ui_meta->type != JSON_OBJECT) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'",
                 tm_type_name(ui_meta));
        *raise = tm_err_spec("AttributeError", msg);
        return NULL;
    }
    const json_t *bots = json_obj_get(ui_meta, "hermes-bots");
    if (bots && bots->type == JSON_OBJECT) return json_obj_get(bots, "title");
    return NULL;
}

/* PoP: tm_bots_title @ tui_gateway/methods_profiles.py:_bots_title */
static json_t *tm_bots_title(tm_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *raise = NULL;
    const json_t *t = tm_bots_title_v(json_obj_get(args, "ui_meta"), &raise);
    if (raise) return raise;
    return t ? json_copy(t) : json_null();
}

/* {k: (e.get("enabled"), e.get("disabled", _ABSENT)) for k, e in
 *  servers.items() if isinstance(e, dict)}; non-dict servers pass through. */
static json_t *tm_mcp_toggle_state_v(const json_t *servers) {
    if (!servers || servers->type != JSON_OBJECT)
        return servers ? json_copy(servers) : json_null();
    json_t *out = json_object();
    for (size_t i = 0; i < servers->c.count; i++) {
        const json_t *e = servers->c.items[i];
        if (!e || e->type != JSON_OBJECT) continue;
        json_t *pair = json_array();
        const json_t *en = json_obj_get(e, "enabled");
        json_append(pair, en ? json_copy(en) : json_null());
        const json_t *dis = json_obj_get(e, "disabled");
        json_append(pair, dis ? json_copy(dis) : json_string("<absent>"));
        json_set(out, servers->c.keys[i], pair);
    }
    return out;
}

/* PoP: tm_mcp_toggle_state @ tui_gateway/methods_profiles.py:_mcp_toggle_state */
static json_t *tm_mcp_toggle_state(tm_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return tm_mcp_toggle_state_v(json_obj_get(args, "servers"));
}

/* ── best-effort wrappers (fn is a scripted callback seam) ── */

/* PoP: tm_try @ tui_gateway/methods_profiles.py:_try */
static json_t *tm_try(tm_ctx_t *ctx, const json_t *args) {
    const json_t *def = json_obj_get(args, "default");
    json_t *r = tm_call(ctx, "try_fn", tm_args0());
    if (tm_is_raise(r)) {
        if (tm_is_base_exc(tm_raise_kind(r))) return r;
        json_free(r);
        return def ? json_copy(def) : json_null();
    }
    return r;
}

/* PoP: tm_best_effort @ tui_gateway/methods_profiles.py:_best_effort */
static json_t *tm_best_effort(tm_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *r = tm_call(ctx, "effort_fn", tm_args0());
    if (tm_is_raise(r)) {
        if (tm_is_base_exc(tm_raise_kind(r))) return r;
        json_free(r);
        return json_bool(false);
    }
    json_free(r);
    return json_bool(true);
}

/* ── late-bound import + model pin ── */

/* _lazy(module, name): getattr(__import__(module, fromlist=[name]), name) */
static json_t *tm_lazy_call(tm_ctx_t *ctx, const char *module, const char *name) {
    json_t *a = tm_args0();
    tm_asets(a, "module", module);
    tm_asets(a, "name", name);
    return tm_call(ctx, "import_attr", a);
}

/* PoP: tm_lazy @ tui_gateway/methods_profiles.py:_lazy */
static json_t *tm_lazy(tm_ctx_t *ctx, const json_t *args) {
    return tm_lazy_call(ctx, json_get_str(args, "module", ""),
                        json_get_str(args, "name", ""));
}

/* _pin_profile_model: the write_profile_model call result is DISCARDED
 * (expression statement); the function returns None. */
static json_t *tm_pin_profile_model_call(tm_ctx_t *ctx, const char *profile_dir,
                                         const char *provider, const char *model,
                                         const char *base_url) {
    json_t *w = tm_lazy_call(ctx, "hermes_cli.profile_provision", "write_profile_model");
    if (tm_is_raise(w)) return w;
    json_free(w);
    json_t *a = tm_args0();
    tm_asets(a, "profile_dir", profile_dir);
    tm_asets(a, "provider", provider);
    tm_asets(a, "model", model);
    tm_asets(a, "base_url", base_url ? base_url : "");
    json_t *r = tm_call(ctx, "write_profile_model", a);
    if (tm_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* PoP: tm_pin_profile_model @ tui_gateway/methods_profiles.py:_pin_profile_model */
static json_t *tm_pin_profile_model(tm_ctx_t *ctx, const json_t *args) {
    return tm_pin_profile_model_call(ctx, json_get_str(args, "profile_dir", ""),
                                     json_get_str(args, "provider", ""),
                                     json_get_str(args, "model", ""),
                                     json_get_str(args, "base_url", ""));
}

/* ── home scope + profile resolution ── */

/* _hermes_home_scope(path): profile_config_scope(path, launch_home=_launch_home()).
 * Evaluation order: the _launch_home() kwarg seam fires BEFORE the
 * profile_config_scope call seam. The returned CM marker's enter/exit are
 * silent; a null marker raises TypeError at the live `with` site. */
static json_t *tm_hermes_home_scope_call(tm_ctx_t *ctx, const char *path) {
    json_t *h = tm_call(ctx, "launch_home", tm_args0());
    if (tm_is_raise(h)) return h;
    json_t *a = tm_args0();
    tm_asets(a, "path", path);
    json_set(a, "launch_home", h); /* adopted; null dropped by tm_call */
    return tm_call(ctx, "profile_config_scope", a);
}

/* PoP: tm_hermes_home_scope @ tui_gateway/methods_profiles.py:_hermes_home_scope */
static json_t *tm_hermes_home_scope(tm_ctx_t *ctx, const json_t *args) {
    return tm_hermes_home_scope_call(ctx, json_get_str(args, "path", ""));
}

/* PoP: tm_resolve_profile @ tui_gateway/methods_profiles.py:_resolve_profile */
static json_t *tm_resolve_profile(tm_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    char *name = tm_str_strip_param(params, "name");
    json_t *out = json_array();
    json_append(out, json_string(name ? name : ""));
    if (!name || !name[0]) {
        free(name);
        json_t *e = tm_err(ctx, rid, 4063, "name required");
        if (tm_is_raise(e)) { json_free(out); return e; }
        json_append(out, json_null());
        json_append(out, e);
        return out;
    }
    json_t *a = tm_args0();
    tm_asets(a, "name", name);
    json_t *g = tm_call(ctx, "get_profile_dir", a);
    if (tm_is_raise(g)) {
        const char *kind = tm_raise_kind(g);
        if (kind && !strcmp(kind, "ValueError")) {
            json_free(g);
            char msg[512];
            snprintf(msg, sizeof msg, "profile '%s' not found", name);
            json_t *e = tm_err(ctx, rid, 4064, msg);
            if (tm_is_raise(e)) { free(name); json_free(out); return e; }
            json_append(out, json_null());
            json_append(out, e);
            free(name);
            return out;
        }
        free(name);
        json_free(out);
        return g;
    }
    /* Path(get_profile_dir(name)) — fixture domain: a POSIX path string. */
    const char *path = tm_str(g);
    char *path_s = tm_strdup(path ? path : "");
    json_free(g);
    a = tm_args0();
    tm_asets(a, "path", path_s);
    json_t *d = tm_call(ctx, "is_dir", a);
    if (tm_is_raise(d)) { free(name); free(path_s); json_free(out); return d; }
    bool is_dir = tm_truthy(d);
    json_free(d);
    if (!is_dir) {
        char msg[512];
        snprintf(msg, sizeof msg, "profile '%s' not found", name);
        json_t *e = tm_err(ctx, rid, 4064, msg);
        if (tm_is_raise(e)) { free(name); free(path_s); json_free(out); return e; }
        json_append(out, json_null());
        json_append(out, e);
        free(name);
        free(path_s);
        return out;
    }
    json_append(out, json_string(path_s));
    json_append(out, json_null());
    free(name);
    free(path_s);
    return out;
}

/* ── profile.yaml read ── */

/* _read_profile_yaml: {} when missing/unreadable/unparseable/not a mapping.
 * Every failure inside load() is swallowed by _try (Exception only). */
static json_t *tm_read_profile_yaml_call(tm_ctx_t *ctx, const char *profile_dir) {
    char *path = tm_path_join(profile_dir, "profile.yaml");
    json_t *loaded = NULL;
    json_t *a = tm_args0();
    tm_asets(a, "path", path);
    json_t *f = tm_call(ctx, "is_file", a);
    if (tm_is_raise(f)) {
        if (tm_is_base_exc(tm_raise_kind(f))) { free(path); return f; }
        json_free(f);
        free(path);
        return json_object();
    }
    if (tm_truthy(f)) {
        a = tm_args0();
        tm_asets(a, "path", path);
        tm_asets(a, "encoding", "utf-8");
        json_t *t = tm_call(ctx, "read_text", a);
        if (tm_is_raise(t)) {
            if (tm_is_base_exc(tm_raise_kind(t))) { json_free(f); free(path); return t; }
            json_free(t);
            loaded = json_object();
        } else {
            a = tm_args0();
            json_set(a, "text", t); /* adopted */
            json_t *y = tm_call(ctx, "yaml_safe_load", a);
            if (tm_is_raise(y)) {
                if (tm_is_base_exc(tm_raise_kind(y))) {
                    json_free(f); free(path); return y;
                }
                json_free(y);
                loaded = json_object();
            } else if (tm_truthy(y)) {
                loaded = y; /* `or {}` keeps a truthy result */
            } else {
                json_free(y);
                loaded = json_object();
            }
        }
    } else {
        loaded = json_object();
    }
    json_free(f);
    free(path);
    if (!loaded || loaded->type != JSON_OBJECT) {
        json_free(loaded);
        loaded = json_object();
    }
    return loaded;
}

/* PoP: tm_read_profile_yaml @ tui_gateway/methods_profiles.py:_read_profile_yaml */
static json_t *tm_read_profile_yaml(tm_ctx_t *ctx, const json_t *args) {
    return tm_read_profile_yaml_call(ctx, json_get_str(args, "profile_dir", ""));
}

/* ── roster session readers ── */

static json_t *tm_latest_message_preview_call(tm_ctx_t *ctx, const json_t *persistence,
                                              const json_t *session_id) {
    json_t *pc = tm_persist_check(persistence, "messages");
    if (pc) {
        /* except Exception -> "" (AttributeError is never a BaseException) */
        json_free(pc);
        return json_string("");
    }
    json_t *a = tm_args0();
    tm_aset(a, "session_id", session_id);
    json_t *c = tm_call(ctx, "latest_dialogue_message", a);
    if (tm_is_raise(c)) {
        if (tm_is_base_exc(tm_raise_kind(c))) return c;
        json_free(c);
        return json_string("");
    }
    char *s = tm_truthy(c) ? tm_pystr(c) : tm_strdup("");
    json_free(c);
    char *text = tm_split_join(s);
    free(s);
    json_t *ret;
    size_t n = strlen(text);
    if (n > 80) {
        char *cut = malloc(84);
        if (!cut) { free(text); return tm_err_spec("MemoryError", ""); }
        memcpy(cut, text, 80);
        memcpy(cut + 80, "...", 4);
        ret = json_string(cut);
        free(cut);
    } else {
        ret = json_string(text);
    }
    free(text);
    return ret;
}

/* PoP: tm_latest_message_preview @ tui_gateway/methods_profiles.py:_latest_message_preview */
static json_t *tm_latest_message_preview(tm_ctx_t *ctx, const json_t *args) {
    return tm_latest_message_preview_call(ctx, json_obj_get(args, "persistence"),
                                          json_obj_get(args, "session_id"));
}

/* _display_count_field: {"live_message_count": n}, {} on STORE_FAILURES.
 * Other exceptions (BaseException included) propagate. */
static json_t *tm_display_count_field_call(tm_ctx_t *ctx, const json_t *persistence,
                                           const json_t *session_id) {
    json_t *pc = tm_persist_check(persistence, "messages");
    if (pc) return pc;
    char *sid = tm_pystr(session_id);
    json_t *a = tm_args0();
    tm_asets(a, "session_id", sid);
    free(sid);
    json_t *n = tm_call(ctx, "display_count", a);
    if (tm_is_raise(n)) {
        if (tm_is_store_failure(tm_raise_kind(n))) {
            json_free(n);
            return json_object();
        }
        return n;
    }
    json_t *out = json_object();
    json_set(out, "live_message_count", n); /* adopted */
    return out;
}

/* PoP: tm_display_count_field @ tui_gateway/methods_profiles.py:_display_count_field */
static json_t *tm_display_count_field(tm_ctx_t *ctx, const json_t *args) {
    return tm_display_count_field_call(ctx, json_obj_get(args, "persistence"),
                                       json_obj_get(args, "session_id"));
}

/* PoP: tm_live_count_field @ tui_gateway/methods_profiles.py:_live_count_field */
static json_t *tm_live_count_field(tm_ctx_t *ctx, const json_t *args) {
    json_t *a = tm_args0();
    tm_aset(a, "db", json_obj_get(args, "db"));
    json_t *p = tm_call(ctx, "persistence_for", a);
    if (tm_is_raise(p)) return p;
    if (!p || p->type == JSON_NULL) {
        json_t *e = tm_cm_type_error(p);
        json_free(p);
        return e;
    }
    /* with block: enter/exit silent */
    json_t *r = tm_display_count_field_call(ctx, p, json_obj_get(args, "session_id"));
    json_free(p);
    return r;
}

/* ── MCP toggles / toolset pin / config landing ── */

/* PoP: tm_mcp_entry_enabled @ tui_gateway/methods_profiles.py:_mcp_entry_enabled */
static json_t *tm_mcp_entry_enabled(tm_ctx_t *ctx, const json_t *args) {
    json_t *a = tm_args0();
    tm_aset(a, "entry", json_obj_get(args, "entry"));
    return tm_call(ctx, "mcp_server_enabled", a);
}

/* PoP: tm_save_toolset_pin @ tui_gateway/methods_profiles.py:_save_toolset_pin */
static json_t *tm_save_toolset_pin(tm_ctx_t *ctx, const json_t *args) {
    json_t *cfg = tm_state_key(ctx, "cfg");
    json_t *wanted = tm_clean_names_v(json_obj_get(args, "enabled"));
    if (json_len(wanted) > 0) {
        json_t *a = tm_args0();
        tm_aset(a, "cfg", cfg);
        tm_asets(a, "platform", "cli");
        json_set(a, "wanted", json_copy(wanted));
        json_t *r = tm_call(ctx, "save_platform_tools", a);
        if (tm_is_raise(r)) { json_free(wanted); return r; }
        json_free(r);
    } else {
        json_t *pt = json_obj_get(cfg, "platform_toolsets");
        if (pt && pt->type == JSON_OBJECT) json_obj_del(pt, "cli");
    }
    json_free(wanted);
    json_t *a = tm_args0();
    tm_aset(a, "cfg", cfg);
    json_t *r = tm_call(ctx, "save_config", a);
    if (tm_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* PoP: tm_save_mcp_toggles @ tui_gateway/methods_profiles.py:_save_mcp_toggles */
static json_t *tm_save_mcp_toggles(tm_ctx_t *ctx, const json_t *args) {
    json_t *cfg = tm_state_key(ctx, "cfg");
    const json_t *launch_mcp = json_obj_get(args, "launch_mcp");
    json_t *wanted = tm_clean_names_v(json_obj_get(args, "enabled"));
    json_t *existing = json_obj_get(cfg, "mcp_servers");
    bool is_child = existing && existing->type == JSON_OBJECT;
    json_t *mc = is_child ? existing : json_object();
    /* for srv in wanted: copy undefined servers from the launch catalog.
     * Fixtures only copy with a ONE-member wanted (set order unobservable). */
    for (size_t i = 0; i < json_len(wanted); i++) {
        const char *srv = tm_str(json_get(wanted, i));
        if (!srv) continue;
        const json_t *e = json_obj_get(mc, srv);
        if (e && e->type == JSON_OBJECT) continue; /* short-circuit */
        if (!launch_mcp || launch_mcp->type != JSON_OBJECT) {
            char msg[160];
            snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'",
                     tm_type_name(launch_mcp));
            json_free(wanted);
            if (!is_child) json_free(mc);
            return tm_err_spec("AttributeError", msg);
        }
        const json_t *le = json_obj_get(launch_mcp, srv);
        if (le && le->type == JSON_OBJECT) json_set(mc, srv, json_copy(le));
    }
    /* for srv, entry in mcp_cfg.items(): dict insertion order */
    for (size_t i = 0; i < mc->c.count; i++) {
        const json_t *entry = mc->c.items[i];
        if (!entry || entry->type != JSON_OBJECT) continue;
        json_t *a = tm_args0();
        tm_aset(a, "entry", entry);
        tm_asetb(a, "enabled", tm_names_contains(wanted, mc->c.keys[i]));
        json_t *r = tm_call(ctx, "write_mcp_server_enabled", a);
        if (tm_is_raise(r)) {
            json_free(wanted);
            if (!is_child) json_free(mc);
            return r;
        }
        json_free(r);
    }
    if (mc->c.count > 0) {
        if (!is_child) json_set(cfg, "mcp_servers", mc); /* adopted */
    } else if (!is_child) {
        json_free(mc);
    }
    json_free(wanted);
    json_t *a = tm_args0();
    tm_aset(a, "cfg", cfg);
    json_t *r = tm_call(ctx, "save_config", a);
    if (tm_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* PoP: tm_unlink_asset_files @ tui_gateway/methods_profiles.py:_unlink_asset_files */
static json_t *tm_unlink_asset_files(tm_ctx_t *ctx, const json_t *args) {
    const char *assets_dir = json_get_str(args, "assets_dir", "");
    const char *asset = json_get_str(args, "asset", "");
    static const char *exts[] = {"png", "jpg", "webp"}; /* _ASSET_EXTS order */
    char *present[3];
    size_t np = 0;
    for (size_t i = 0; i < 3; i++) {
        char leaf[256];
        snprintf(leaf, sizeof leaf, "%s.%s", asset, exts[i]);
        char *p = tm_path_join(assets_dir, leaf);
        json_t *a = tm_args0();
        tm_asets(a, "path", p);
        json_t *f = tm_call(ctx, "is_file", a);
        if (tm_is_raise(f)) {
            for (size_t j = 0; j < np; j++) free(present[j]);
            free(p);
            return f;
        }
        if (tm_truthy(f)) present[np++] = p;
        else free(p);
        json_free(f);
    }
    size_t count = 0;
    for (size_t i = 0; i < np; i++) {
        json_t *a = tm_args0();
        tm_asets(a, "path", present[i]);
        json_t *u = tm_call(ctx, "unlink", a);
        if (tm_is_raise(u)) {
            for (size_t j = 0; j < np; j++) free(present[j]);
            return u;
        }
        json_free(u);
        count++;
    }
    for (size_t j = 0; j < np; j++) free(present[j]);
    return json_number((double)count);
}

/* _landed's at(doc): walk the path; non-dict/missing -> None. */
static const json_t *tm_at(const json_t *doc, const json_t *path) {
    const json_t *node = doc;
    if (!path || path->type != JSON_ARRAY) return node;
    for (size_t i = 0; i < json_len(path); i++) {
        const char *seg = tm_str(json_get(path, i));
        if (!node || node->type != JSON_OBJECT || !seg) return NULL;
        const json_t *v = json_obj_get(node, seg);
        if (!v) return NULL;
        node = v;
    }
    return node;
}

/* PoP: tm_landed @ tui_gateway/methods_profiles.py:_landed */
static json_t *tm_landed(tm_ctx_t *ctx, const json_t *args) {
    const json_t *cfg = json_obj_get(args, "cfg");
    const json_t *path = json_obj_get(args, "path");
    const char *key = json_get_str(args, "key", "identity");
    bool use_toggle = key && !strcmp(key, "mcp_toggle_state");
    json_t *raw = tm_call(ctx, "read_user_config_raw", tm_args0());
    if (tm_is_raise(raw)) return raw;
    json_t *doc;
    if (tm_truthy(raw)) doc = raw;
    else { json_free(raw); doc = json_object(); }
    const json_t *a = tm_at(doc, path);
    const json_t *b = tm_at(cfg, path);
    json_t *ka, *kb;
    if (use_toggle) {
        ka = tm_mcp_toggle_state_v(a);
        kb = tm_mcp_toggle_state_v(b);
    } else {
        ka = a ? json_copy(a) : json_null();
        kb = b ? json_copy(b) : json_null();
    }
    bool eq = tm_json_eq(ka, kb);
    json_free(ka);
    json_free(kb);
    json_free(doc);
    return json_bool(eq);
}

/* ── model-pin confirm handshake ── */

/* _configure_model body; returns the confirm message (owned) or NULL (None),
 * or a raise spec. applied is the caller dict (state mirror). */
static json_t *tm_configure_model_body(tm_ctx_t *ctx, const json_t *params,
                                       const char *profile_dir, json_t *applied) {
    /* LIVE wraps profile_dir in Path(); Path("") == Path(".") whose
     * .as_posix() is "." — map an empty dir before any path seam. */
    const char *pd = (profile_dir && profile_dir[0]) ? profile_dir : ".";
    char *model = tm_str_strip_param(params, "model");
    char *provider = tm_str_strip_param(params, "provider");
    if (!model[0] || !provider[0]) {
        free(model);
        free(provider);
        return NULL;
    }
    json_t *confirm = NULL;
    const json_t *cev = json_obj_get(params, "confirm_expensive_model");
    json_t *cev_free = NULL;
    if (!cev) { cev = cev_free = json_bool(false); } /* params.get(k, False) */
    bool confirmed = tm_is_truthy_value(cev, false);
    json_free(cev_free);
    if (!confirmed) {
        json_t *warn = tm_lazy_call(ctx, "hermes_cli.model_selection_guards",
                                    "combined_selection_warning");
        if (tm_is_raise(warn)) { free(model); free(provider); return warn; }
        json_free(warn); /* the callable marker; the call is its own seam */
        json_t *a = tm_args0();
        tm_asets(a, "profile_dir", pd);
        tm_asets(a, "target_model", model);
        json_t *context = tm_call(ctx, "profile_group_selection_context", a);
        if (tm_is_raise(context)) {
            if (tm_is_base_exc(tm_raise_kind(context))) {
                free(model); free(provider); return context;
            }
            json_free(context);
            context = NULL; /* _try(..., None) */
        }
        json_t *cm = tm_hermes_home_scope_call(ctx, pd);
        if (tm_is_raise(cm)) {
            json_free(context); free(model); free(provider); return cm;
        }
        if (!cm || cm->type == JSON_NULL) {
            json_t *e = tm_cm_type_error(cm);
            json_free(cm); json_free(context); free(model); free(provider);
            return e;
        }
        /* with block: warn(model, provider=provider, selection_context=context) */
        a = tm_args0();
        tm_asets(a, "model", model);
        tm_asets(a, "provider", provider);
        tm_aset(a, "selection_context", context);
        json_free(context);
        json_t *w = tm_call(ctx, "combined_selection_warning", a);
        if (tm_is_raise(w)) {
            if (tm_is_base_exc(tm_raise_kind(w))) {
                json_free(cm); free(model); free(provider); return w;
            }
            json_free(w); /* _try(..., None) swallows */
        } else {
            if (w && w->type == JSON_OBJECT) {
                const json_t *m = json_obj_get(w, "message");
                confirm = m ? json_copy(m) : NULL;
            }
            json_free(w);
        }
        json_free(cm); /* exit silent */
    }
    if (!confirm) {
        /* applied["model"] = _best_effort(_pin_profile_model(...)) */
        json_t *r = tm_pin_profile_model_call(ctx, pd, provider, model, "");
        bool effort;
        if (tm_is_raise(r)) {
            if (tm_is_base_exc(tm_raise_kind(r))) {
                free(model); free(provider); return r;
            }
            json_free(r);
            effort = false;
        } else {
            json_free(r);
            effort = true;
        }
        json_set(applied, "model", json_bool(effort));
    }
    free(model);
    free(provider);
    return confirm;
}

/* PoP: tm_configure_model @ tui_gateway/methods_profiles.py:_configure_model */
static json_t *tm_configure_model(tm_ctx_t *ctx, const json_t *args) {
    json_t *applied = tm_state_key(ctx, "applied");
    const json_t *params = json_obj_get(args, "params");
    const char *profile_dir = json_get_str(args, "profile_dir", "");
    return tm_configure_model_body(ctx, params, profile_dir, applied);
}

/* ── ui_meta CAS writer ── */

static json_t *tm_configure_ui_meta_body(tm_ctx_t *ctx, const json_t *params,
                                         const char *profile_dir, json_t *applied) {
    const json_t *incoming = json_obj_get(params, "ui_meta");
    if (!incoming) return tm_err_spec("KeyError", "'ui_meta'");
    /* Fixture domain: incoming is a dict (the profiles.configure handler gates
     * isinstance(params.get("ui_meta"), dict) before calling). A non-dict is
     * outside the domain; fail closed at the .items() site rather than crash. */
    if (incoming->type != JSON_OBJECT) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'items'",
                 tm_type_name(incoming));
        return tm_err_spec("AttributeError", msg);
    }
    if (tm_dumps_len(incoming) > 65536) return NULL;
    const json_t *expected = json_obj_get(params, "ui_meta_expected_revisions");
    if (expected && expected->type != JSON_NULL && expected->type != JSON_OBJECT)
        return tm_err_spec("ValueError", "ui_meta_expected_revisions must be an object");
    /* with _profile_ui_meta_lock: silent */
    json_t *existing = tm_read_profile_yaml_call(ctx, profile_dir);
    if (tm_is_raise(existing)) return existing; /* BaseException only */
    const json_t *raw_rev = json_obj_get(existing, "_ui_meta_revisions");
    json_t *revisions = tm_clean_revisions_v(
        (raw_rev && raw_rev->type == JSON_OBJECT) ? raw_rev : NULL);
    /* per-key CAS */
    json_t *conflicts = json_object();
    if (expected && expected->type == JSON_OBJECT) {
        for (size_t i = 0; i < incoming->c.count; i++) {
            const char *key = incoming->c.keys[i];
            const json_t *wanted = json_obj_get(expected, key);
            const json_t *av = json_obj_get(revisions, key);
            double actual = av ? av->num_val : 0.0;
            bool ok = wanted && wanted->type == JSON_NUMBER
                && wanted->num_val == (double)(long long)wanted->num_val
                && wanted->num_val >= 0.0 && wanted->num_val == actual;
            if (!ok) {
                json_t *c = json_object();
                json_set(c, "expected", wanted ? json_copy(wanted) : json_null());
                json_set(c, "actual", json_number(actual));
                json_set(conflicts, key, c);
            }
        }
    }
    if (conflicts->c.count > 0) {
        json_set(applied, "ui_meta_conflicts", conflicts);
        json_t *rev_out = json_object();
        for (size_t i = 0; i < incoming->c.count; i++) {
            const json_t *av = json_obj_get(revisions, incoming->c.keys[i]);
            json_set(rev_out, incoming->c.keys[i],
                     json_number(av ? av->num_val : 0.0));
        }
        json_set(applied, "ui_meta_revisions", rev_out);
        json_free(revisions);
        json_free(existing);
        return NULL;
    }
    json_free(conflicts);
    /* merge into a working copy of current (live mutates existing's child in
     * place; the copy keeps ownership simple and the wire value identical) */
    const json_t *cur_src = json_obj_get(existing, "ui_meta");
    json_t *current = (cur_src && cur_src->type == JSON_OBJECT)
        ? json_copy(cur_src) : json_object();
    json_t *raise = NULL;
    const json_t *old_t = tm_bots_title_v(current, &raise);
    json_t *old_title = old_t ? json_copy(old_t) : NULL;
    for (size_t i = 0; i < incoming->c.count; i++) {
        const char *key = incoming->c.keys[i];
        const json_t *value = incoming->c.items[i];
        if (value && value->type == JSON_NULL) json_obj_del(current, key);
        else json_set(current, key, json_copy(value));
        const json_t *rv = json_obj_get(revisions, key);
        json_set(revisions, key, json_number((rv ? rv->num_val : 0.0) + 1.0));
    }
    if (current->c.count > 0) {
        json_set(existing, "ui_meta", current); /* adopted (replaces child) */
    } else {
        json_obj_del(existing, "ui_meta");
        /* current stays alive for the title check below */
    }
    json_set(existing, "_ui_meta_revisions", revisions); /* adopted */
    char *ypath = tm_path_join(profile_dir, "profile.yaml");
    json_t *a = tm_args0();
    tm_asets(a, "path", ypath);
    free(ypath);
    tm_aset(a, "doc", existing);
    tm_asetb(a, "sort_keys", false);
    json_t *r = tm_call(ctx, "atomic_yaml_write", a);
    if (tm_is_raise(r)) {
        json_free(old_title);
        if (current->c.count == 0) json_free(current);
        json_free(existing);
        return r;
    }
    json_free(r);
    json_set(applied, "ui_meta", json_bool(true));
    json_t *rev_out = json_object();
    for (size_t i = 0; i < incoming->c.count; i++) {
        const json_t *rv = json_obj_get(revisions, incoming->c.keys[i]);
        json_set(rev_out, incoming->c.keys[i], rv ? json_copy(rv) : json_number(0));
    }
    json_set(applied, "ui_meta_revisions", rev_out);
    const json_t *new_t = tm_bots_title_v(current, &raise);
    bool changed = !tm_json_eq(old_title, new_t);
    if (changed) {
        /* logging.getLogger(__name__).info(fmt, name, old, new, revision) */
        const json_t *nm = json_obj_get(params, "name");
        char *name_s = tm_truthy(nm) ? tm_pystr(nm) : tm_path_name(profile_dir);
        char *old_r = tm_repr(old_title);
        char *new_r = tm_repr(new_t);
        const json_t *rv = json_obj_get(revisions, "hermes-bots");
        char *rev_s = tm_pystr(rv);
        size_t cap = strlen(name_s) + strlen(old_r) + strlen(new_r)
            + strlen(rev_s) + 96;
        char *msg = malloc(cap);
        if (msg) {
            snprintf(msg, cap,
                     "ui_meta hermes-bots title for profile %s: %s -> %s (revision %s)",
                     name_s, old_r, new_r, rev_s);
            a = tm_args0();
            tm_asets(a, "msg", msg);
            free(msg);
            json_t *lr = tm_call(ctx, "logger_info", a);
            if (tm_is_raise(lr)) {
                free(name_s); free(old_r); free(new_r); free(rev_s);
                json_free(old_title);
                if (current->c.count == 0) json_free(current);
                json_free(existing);
                return lr;
            }
            json_free(lr);
        }
        free(name_s); free(old_r); free(new_r); free(rev_s);
    }
    json_free(old_title);
    if (current->c.count == 0) json_free(current);
    json_free(existing);
    return NULL;
}

/* PoP: tm_configure_ui_meta @ tui_gateway/methods_profiles.py:_configure_ui_meta */
static json_t *tm_configure_ui_meta(tm_ctx_t *ctx, const json_t *args) {
    json_t *applied = tm_state_key(ctx, "applied");
    json_set(applied, "ui_meta", json_bool(false));
    const json_t *params = json_obj_get(args, "params");
    const char *profile_dir = json_get_str(args, "profile_dir", "");
    json_t *r = tm_configure_ui_meta_body(ctx, params, profile_dir, applied);
    if (r) {
        if (tm_is_base_exc(tm_raise_kind(r))) return r;
        json_free(r);
        json_set(applied, "ui_meta", json_bool(false));
    }
    return NULL;
}

/* ── canonical-row resurrection ── */

static const char *tm_recoverable_reasons[] = {
    "agent_close", "ws_orphan_reap", "superseded_by_resume", "startup_orphan_reap"
};

static json_t *tm_resurrect_body(tm_ctx_t *ctx, const json_t *persistence,
                                 const char *profile_path, const char *session_id) {
    json_t *pc = tm_persist_check(persistence, "sessions");
    if (pc) return pc;
    json_t *a = tm_args0();
    tm_asets(a, "session_id", session_id);
    json_t *row = tm_call(ctx, "sessions_get", a);
    if (tm_is_raise(row)) return row;
    if (!tm_double_truthy(row)) { json_free(row); return json_bool(false); }
    json_t *raise = NULL;
    const json_t *arch = tm_row_attr(row, "archived", false, NULL, &raise);
    if (raise) { json_free(row); return raise; }
    if (!tm_truthy(arch)) { json_free(row); return json_bool(false); }
    a = tm_args0();
    tm_asets(a, "session_id", session_id);
    json_t *t = tm_call(ctx, "lineage_tip", a);
    if (tm_is_raise(t)) {
        if (tm_is_base_exc(tm_raise_kind(t))) { json_free(row); return t; }
        json_free(t);
        t = NULL; /* _try(..., None) */
    }
    char *tip_id = (t && tm_truthy(t)) ? tm_pystr(t) : tm_strdup(session_id);
    json_free(t);
    json_t *g = NULL;
    const json_t *tip;
    if (strcmp(tip_id, session_id) != 0) {
        a = tm_args0();
        tm_asets(a, "session_id", tip_id);
        g = tm_call(ctx, "sessions_get", a);
        if (tm_is_raise(g)) { free(tip_id); json_free(row); return g; }
        tip = tm_double_truthy(g) ? g : row;
    } else {
        tip = row;
    }
    const json_t *er = tm_row_attr(tip, "end_reason", false, NULL, &raise);
    if (raise) {
        free(tip_id); json_free(g); json_free(row); return raise;
    }
    char *er_s = tm_truthy(er) ? tm_pystr(er) : tm_strdup("");
    bool recoverable = false;
    for (size_t i = 0; i < 4; i++)
        if (!strcmp(er_s, tm_recoverable_reasons[i])) { recoverable = true; break; }
    free(er_s);
    if (!recoverable) {
        free(tip_id); json_free(g); json_free(row);
        return json_bool(false);
    }
    a = tm_args0();
    tm_asets(a, "home", profile_path);
    json_t *scope = tm_call(ctx, "profile_scope", a);
    if (tm_is_raise(scope)) { free(tip_id); json_free(g); json_free(row); return scope; }
    a = tm_args0();
    json_set(a, "scope", scope); /* adopted */
    json_t *writer = tm_call(ctx, "open_persistence", a);
    if (tm_is_raise(writer)) { free(tip_id); json_free(g); json_free(row); return writer; }
    /* inner try: return bool(writer.sessions.unarchive_recoverable(sid))
     * finally: _best_effort(writer.close) */
    json_t *un;
    if (writer && writer->type == JSON_OBJECT) {
        a = tm_args0();
        tm_asets(a, "session_id", session_id);
        un = tm_call(ctx, "unarchive_recoverable", a);
    } else {
        /* writer.sessions attribute access raises BEFORE any seam fires */
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'sessions'",
                 tm_type_name(writer));
        un = tm_err_spec("AttributeError", msg);
    }
    json_t *fin_raise = NULL;
    if (writer && writer->type == JSON_OBJECT) {
        json_t *wc = tm_call(ctx, "writer_close", tm_args0());
        if (tm_is_raise(wc)) {
            if (tm_is_base_exc(tm_raise_kind(wc))) fin_raise = wc;
            else json_free(wc); /* _best_effort swallows Exception */
        } else {
            json_free(wc);
        }
    }
    /* else: writer.close attribute access raises inside _best_effort's fn —
     * swallowed, NO seam event (the recorder never runs). */
    json_free(writer);
    free(tip_id);
    json_free(g);
    json_free(row);
    if (fin_raise) { json_free(un); return fin_raise; }
    if (tm_is_raise(un)) return un;
    json_t *ret = json_bool(tm_truthy(un));
    json_free(un);
    return ret;
}

/* PoP: tm_resurrect_recoverable_canonical @ tui_gateway/methods_profiles.py:_resurrect_recoverable_canonical */
static json_t *tm_resurrect_recoverable_canonical(tm_ctx_t *ctx, const json_t *args) {
    json_t *r = tm_resurrect_body(ctx, json_obj_get(args, "persistence"),
                                  json_get_str(args, "profile_path", ""),
                                  json_get_str(args, "session_id", ""));
    if (tm_is_raise(r)) {
        if (tm_is_base_exc(tm_raise_kind(r))) return r;
        json_free(r);
        return json_bool(false);
    }
    return r;
}

/* ── canonical session row ── */

static json_t *tm_canonical_body(tm_ctx_t *ctx, const json_t *persistence,
                                 const char *profile_path) {
    char *sid = NULL, *tip = NULL;
    json_t *row = NULL, *g = NULL, *started = NULL, *root_title = NULL;
    json_t *title = NULL, *preview = NULL, *tip_node = NULL, *started_at = NULL;
    json_t *last_active = NULL, *message_count = NULL, *dc = NULL, *err = NULL;
    json_t *raise = NULL;
    json_t *pc = tm_persist_check(persistence, "sessions");
    if (pc) return pc;
    json_t *a = tm_args0();
    tm_asets(a, "title", "Bot Chat");
    row = tm_call(ctx, "get_by_title", a);
    if (tm_is_raise(row)) { err = row; row = NULL; goto cleanup; }
    const json_t *sid_v = tm_row_attr(row, "session_id", true, NULL, &raise);
    if (raise) { err = raise; goto cleanup; }
    char *sid_s = tm_truthy(sid_v) ? tm_pystr(sid_v) : tm_strdup("");
    sid = tm_strip(sid_s);
    free(sid_s);
    if (!sid[0]) goto cleanup; /* err NULL -> None */
    const json_t *src = tm_row_attr(row, "source", false, NULL, &raise);
    if (raise) { err = raise; goto cleanup; }
    a = tm_args0();
    json_t *srow = json_object();
    tm_aset(srow, "source", src);
    json_set(a, "row", srow);
    json_t *d = tm_call(ctx, "denied_source", a);
    if (tm_is_raise(d)) { err = d; goto cleanup; }
    bool denied = tm_truthy(d);
    json_free(d);
    if (denied) goto cleanup;
    const json_t *arch = tm_row_attr(row, "archived", false, NULL, &raise);
    if (raise) { err = raise; goto cleanup; }
    if (tm_truthy(arch)) {
        a = tm_args0();
        tm_asets(a, "profile_path", profile_path);
        tm_asets(a, "session_id", sid);
        json_t *rr = tm_call(ctx, "resurrect_recoverable_canonical", a);
        if (tm_is_raise(rr)) { err = rr; goto cleanup; }
        bool ok = tm_truthy(rr);
        json_free(rr);
        if (!ok) goto cleanup;
    }
    a = tm_args0();
    tm_asets(a, "session_id", sid);
    json_t *t = tm_call(ctx, "lineage_tip", a);
    if (tm_is_raise(t)) {
        if (tm_is_base_exc(tm_raise_kind(t))) { err = t; goto cleanup; }
        json_free(t);
        t = NULL;
    }
    tip = (t && tm_truthy(t)) ? tm_pystr(t) : tm_strdup(sid);
    json_free(t);
    a = tm_args0();
    tm_asets(a, "session_id", tip);
    g = tm_call(ctx, "sessions_get", a);
    if (tm_is_raise(g)) { err = g; g = NULL; goto cleanup; }
    const json_t *tip_row = tm_double_truthy(g) ? g : row;
    const json_t *started_v = tm_row_attr(row, "started_at", false, NULL, &raise);
    if (raise) { err = raise; goto cleanup; }
    started = tm_truthy(started_v) ? json_copy(started_v) : json_number(0);
    /* dict literal evaluation order: root_title, title, preview, started_at,
     * last_active, message_count, **_display_count_field */
    const json_t *rt_v = tm_row_attr(row, "title", false, NULL, &raise);
    if (raise) { err = raise; goto cleanup; }
    root_title = tm_truthy(rt_v) ? json_copy(rt_v) : json_string("");
    const json_t *tt_v = tm_row_attr(tip_row, "title", false, NULL, &raise);
    if (raise) { err = raise; goto cleanup; }
    title = tm_truthy(tt_v) ? json_copy(tt_v) : json_string("");
    tip_node = json_string(tip);
    preview = tm_latest_message_preview_call(ctx, persistence, tip_node);
    if (tm_is_raise(preview)) { err = preview; preview = NULL; goto cleanup; }
    const json_t *sa_v = tm_row_attr(tip_row, "started_at", false, NULL, &raise);
    if (raise) { err = raise; goto cleanup; }
    started_at = tm_truthy(sa_v) ? json_copy(sa_v) : json_copy(started);
    const json_t *la_v = tm_row_attr(tip_row, "last_activity_at", false, NULL, &raise);
    if (raise) { err = raise; goto cleanup; }
    if (tm_truthy(la_v)) {
        last_active = json_copy(la_v);
    } else {
        const json_t *sa2 = tm_row_attr(tip_row, "started_at", false, NULL, &raise);
        if (raise) { err = raise; goto cleanup; }
        last_active = tm_truthy(sa2) ? json_copy(sa2) : json_copy(started);
    }
    const json_t *mc_v = tm_row_attr(tip_row, "message_count", false, NULL, &raise);
    if (raise) { err = raise; goto cleanup; }
    message_count = tm_truthy(mc_v) ? json_copy(mc_v) : json_number(0);
    dc = tm_display_count_field_call(ctx, persistence, tip_node);
    if (tm_is_raise(dc)) { err = dc; dc = NULL; goto cleanup; }
    json_t *out = json_object();
    tm_asets(out, "id", sid);
    tm_asets(out, "resolved_id", tip);
    json_set(out, "root_title", root_title);
    json_set(out, "title", title);
    json_set(out, "preview", preview);
    json_set(out, "started_at", started_at);
    json_set(out, "last_active", last_active);
    json_set(out, "message_count", message_count);
    if (dc && dc->type == JSON_OBJECT) {
        for (size_t i = 0; i < dc->c.count; i++)
            json_set(out, dc->c.keys[i], json_copy(dc->c.items[i]));
    }
    /* adopted into out — do not free at cleanup */
    root_title = title = preview = started_at = last_active = message_count = NULL;
    json_free(dc);
    json_free(started);
    free(sid);
    free(tip);
    json_free(row);
    json_free(g);
    json_free(tip_node);
    return out;
cleanup:
    free(sid);
    free(tip);
    json_free(row);
    json_free(g);
    json_free(started);
    json_free(root_title);
    json_free(title);
    json_free(preview);
    json_free(tip_node);
    json_free(started_at);
    json_free(last_active);
    json_free(message_count);
    json_free(dc);
    return err;
}

/* PoP: tm_canonical_session_row @ tui_gateway/methods_profiles.py:_canonical_session_row */
static json_t *tm_canonical_session_row(tm_ctx_t *ctx, const json_t *args) {
    json_t *r = tm_canonical_body(ctx, json_obj_get(args, "persistence"),
                                  json_get_str(args, "profile_path", ""));
    if (tm_is_raise(r)) {
        if (tm_is_base_exc(tm_raise_kind(r))) return r;
        json_free(r);
        return NULL;
    }
    return r;
}