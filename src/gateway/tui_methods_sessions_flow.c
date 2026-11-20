/*
 * tui_methods_sessions_flow.c -- FLOW + leaf port of tui_gateway/methods_session.py
 * (hermes-agent): the session JSON-RPC helper surface -- handler plumbing
 * (_session_arg / _session_method / _with_db), runtime-id minting, profile
 * build scopes, agent-in-context builds, cwd info, listing row summaries,
 * record/row projections, deny-listed listings, live-session snapshots, the
 * pet display/emit/abort/method/selection helpers, base64, billing call /
 * invalid / pending-change / route envelopes, read-only unarchive escalation,
 * exact-title session lookup, live-unpersisted lookup, status rows, title
 * reads, auto-reload and legacy spawn-tree entries.
 *
 * NOTE: the quarry module is tui_gateway/methods_session.py (SINGULAR -- there
 * is no methods_sessions.py); the PoP annotations name the REAL defining
 * module. The deliverable keeps the task's "sessions" spelling.
 *
 * CLAIMED siblings (src/cli/port_methods_session.c) are SEAMED, never
 * re-ported: _str_param ("str_param"), _denied_source ("denied_source").
 * _display_count_field is CLAIMED by src/gateway/tui_methods_flow.c
 * (methods_profiles.py) and seamed ("display_count_field", handle omitted).
 * Ported siblings compose LIVE where they are leaf-flows (_row_of,
 * _unarchive_recoverable, _session_row_summary, _try_get_session,
 * _billing_invalid, _billing_call) -- their seams fire from inside the
 * composer. Decorator factories (_session_arg / _session_method / _with_db /
 * _pet_method / _billing_route) are ported as their generated HANDLER flow;
 * the registry decorators (method / _profile_scoped) are identity at
 * decoration time and fire no events (name / scoped are accepted for
 * fidelity but never seamed).
 *
 * Ports the DECISION SEQUENCES -- branch order, error-code/message composition,
 * seam call order, caller-dict mutation history and exception paths -- not just
 * leaf outputs. Every live external call enters through ONE universal seam:
 * tms2_call(ctx, name, args). The harness wires it to a recording stub fed the
 * case's scripted results (in.io); the oracle installs recording stubs
 * emitting the identical event shape {"name","args","ret"} from the SAME
 * scripts.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and tms2_-prefixed.
 *
 * Server globals are seams: methods_session bodies are rebound onto
 * server.py's globals at install time (method_ctx.bind_module), so _ok / _err
 * / _sessions / _sessions_lock / _sess / _sess_nowait / _session_db /
 * _profile_db / _persistence_for / _db_unavailable_error / _emit / logger /
 * _resolve_session_source / _session_profile_runtime_scope /
 * _set_session_context / _clear_session_context / _make_agent / _session_info
 * / git_probe / _project_info_for_cwd / _pet_cancel_release /
 * _pet_active_selection / _serialize_billing_error are bare free variables.
 * _sessions_lock is a silent lock (no events). _sessions.items() is the
 * "sessions_items" seam (list of [key, record] pairs). uuid.uuid4().hex is
 * the "uuid4_hex" seam (C takes the first 8 chars). DESKTOP_BACKEND_CONTRACT
 * (server.py) and BOT_CHAT_TITLE (tools/bot_mode_probe.py) are seeded
 * constants, AST-extracted by the oracle.
 *
 * Seam name -> live call (args are informational projections):
 *   resolve                 resolve(params, rid) in _session_arg's handler
 *                           -> [session, err]
 *   sess_nowait             _sess_nowait(params, rid) -> [session, err]
 *   sess                    _sess(params, rid) -> [session, err]
 *   session_fn              fn(rid, params, session) callback
 *   session_db              _session_db(session) -> CM marker (dict)
 *   profile_db              _profile_db(params) -> CM marker (dict)
 *   persistence_for         _persistence_for(db) -> CM marker (dict);
 *                           {"cm_enter_none": true} enters to None
 *   db_unavailable_error    _db_unavailable_error(rid, code=code)
 *   db_fn                   fn(rid, params, *session, persistence) callback
 *   uuid4_hex               uuid.uuid4().hex
 *   str_param               _str_param(params, key, default="")  [claimed]
 *   resolve_session_source  _resolve_session_source(source)
 *   session_profile_runtime_scope  _session_profile_runtime_scope(
 *                           {"profile_home": str(home) or None})
 *   set_session_context     _set_session_context(key, cwd=...)
 *   make_agent              _make_agent(sid, key, session_id=key, **kwargs)
 *   clear_session_context   _clear_session_context(tokens)
 *   session_info            _session_info(agent, session)
 *   git_branch              git_probe.branch(cwd)
 *   project_info_for_cwd    _project_info_for_cwd(cwd)
 *   display_count_field     _display_count_field(persistence, sid)
 *                           [claimed sibling; persistence handle omitted]
 *   as_session_row          record.as_session_row()
 *   list_sessions           persistence.sessions.list_sessions(
 *                           SessionQuery(order=LAST_ACTIVE, **kwargs),
 *                           SessionWindow(limit=limit), compact_rows=True)
 *   as_list_row             entry.as_list_row()
 *   denied_source           _denied_source(row)  [claimed]
 *   sessions_items          list(_sessions.items()) -> [[key, record], ...]
 *   err                     _err(rid, code, msg)
 *   ok                      _ok(rid, payload)
 *   load_config             hermes_cli.config.load_config()
 *   emit                    _emit(event, "", payload)
 *   logger_debug            logger.debug(fmt, *args) -> {"msg": fmt % args}
 *   logger_warning          logger.warning(fmt, *args) -> {"msg": fmt % args}
 *   pet_cancel_release      _pet_cancel_release(token)
 *   pet_fn                  fn(rid, params[, slug]) callback in _pet_method
 *   fail_open_fn            fail_open(params) callable in _pet_method
 *   pet_active_selection    _pet_active_selection() -> [enabled, pet, scale]
 *   followup_fn             fn(*args) callback in _pet_config_followup
 *   resolve_provider        imagegen.resolve_provider(require_references=...,
 *                           prefer=name)
 *   billing_fn              fn() callback in _billing_call
 *   serialize_billing_error _serialize_billing_error(exc)
 *                           [exc projected {"__raise__", "message"}]
 *   invalid_fn              invalid(params) callback in _billing_route
 *   new_idempotency_key     agent.billing_view.new_idempotency_key()
 *   route_call              call(nb, params, key) in _billing_route
 *                           [nb module handle omitted]
 *   open_persistence        persistence_assembly.open_persistence(scope,
 *                           eager=True) -> writer handle (dict marker)
 *   unarchive_recoverable   persistence.sessions.unarchive_recoverable(sid)
 *   writer_unarchive        writer.sessions.unarchive_recoverable(sid)
 *   writer_close            writer.close()  [via contextlib.suppress]
 *   get_by_title            persistence.sessions.get_by_title(title)
 *   sessions_get            persistence.sessions.get(session_id)
 *   lineage_tip             persistence.lineage.tip(session_id)
 *   get_title               persistence.sessions.get_title(key)
 *   set_title               persistence.sessions.set_title(key, title)
 *   patch_auto_top_up       nb.patch_auto_top_up(enabled=..., threshold=...,
 *                           top_up_amount=...)  [nb handle omitted]
 *   stat                    Path(path).stat() -> {"st_mtime": ...}
 *   read_text               Path(path).read_text(encoding="utf-8")
 *
 * Representation contract (shared with tests/oracle_tui_methods_sessions_flow.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)}; BaseException-only
 *   kinds (SystemExit, KeyboardInterrupt, GeneratorExit, CancelledError) PASS
 *   THROUGH every except-Exception site. Raise-spec messages ARE str(exc):
 *   KeyError specs carry repr(key) (single-arg KeyError str()).
 * - BillingError is a synthetic Exception subclass (hermes_cli.nous_billing);
 *   _billing_call catches exactly that kind name (subclass raises outside the
 *   fixture domain).
 * - Doubles: scripted dicts become SimpleNamespaces. Attribute reads on a
 *   missing key raise "'types.SimpleNamespace' object has no attribute '<k>'";
 *   on a non-dict receiver "'<type>' object has no attribute '<k>'". A double
 *   is ALWAYS truthy (even {}), modeled by object presence, NOT container
 *   truthiness. Plain dict rows (session / params / row / result) keep
 *   container truthiness and dict subscript semantics (KeyError / TypeError).
 * - Persistence handles: dict scripts with optional "sessions" / "lineage"
 *   sub-objects (method seams fire only when the sub-object key is present),
 *   "read_only" / "scope" plain attributes. Writer handles always carry
 *   sessions.unarchive_recoverable + close. Null handles raise the live
 *   NoneType attribute messages at the access site.
 * - Context-manager markers (session_db / profile_db / persistence_for
 *   scripts): a dict enters to itself (the handle marker); the special key
 *   "cm_enter_none": true enters to None; a null/non-dict marker raises
 *   TypeError at the live `with` site. Silent enter/exit (no events).
 * - Tuples project to lists; integral floats to ints; Paths to POSIX strings.
 *   Fixture paths are absolute POSIX, no trailing slash, no "." components
 *   (str(p) == the fixture string). Fixture numbers are ints.
 * - str()/strip()/lower() fold the ASCII fixture corpus only; the strip
 *   whitespace set includes \x1c-\x1f (Python White_Space). str() sites see
 *   str/int/bool/null only.
 * - Unpacking (resolve / pet_active_selection / sessions pairs): lists and
 *   strings are iterable; wrong length raises ValueError "too many values to
 *   unpack (expected N, got M)" / "not enough values to unpack (expected N,
 *   got M)"; non-iterables raise TypeError "cannot unpack non-iterable X
 *   object". Iteration domains (entries / sessions_items) are lists or null
 *   (null -> TypeError "'NoneType' object is not iterable").
 * - _legacy_spawn_tree_entry's json.loads is transcribed with a STRICT Python
 *   JSON grammar pre-validator (ws set space/tab/nl/cr; no leading zeros, no
 *   trailing commas, no NaN/Infinity, escape set "\\ \/ b f n r t uXXXX")
 *   before the lenient libjson parse; fixture text stays in that domain
 *   (no lone-surrogate \u escapes). Parse failure / non-dict -> raw = {}.
 * - _b64 input is ASCII text interpreted as bytes (standard_b64encode).
 * - State: ctx.state mirrors the caller dict "session" (_title_read),
 *   materialized at entry and mutated in place. Other functions leave state
 *   alone.
 * - Seam args drop None-valued keys on both sides (absent-vs-null parity).
 *
 * Ownership: tms2_call ADOPTS args into the trace event; its return is a fresh
 * owned node. json_set ADOPTS its value and frees the replaced node -- borrowed
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

/* -- universal seam context -- */
typedef struct tms2_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} tms2_ctx_t;

/* -- basic helpers -- */
static bool tms2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static const char *tms2_raise_kind(const json_t *v) {
    return tms2_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}

/* BaseException-only kinds pass through every except-Exception site. */
static bool tms2_is_base_exc(const char *kind) {
    return kind && (!strcmp(kind, "SystemExit") || !strcmp(kind, "KeyboardInterrupt")
                    || !strcmp(kind, "GeneratorExit") || !strcmp(kind, "CancelledError"));
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool tms2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static const char *tms2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static json_t *tms2_err_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* Python type name for a JSON value (fixture scalar/container domain). */
static const char *tms2_type_name(const json_t *v) {
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

static char *tms2_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

/* Python str() for scalars (containers are outside the str()-projection
 * fixture domain). */
static char *tms2_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return tms2_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return tms2_strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return tms2_strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && d > -9.2e18 && d < 9.2e18)
                snprintf(buf, sizeof buf, "%lld", (long long)d);
            else
                snprintf(buf, sizeof buf, "%.17g", d);
            return tms2_strdup(buf);
        }
        default: return json_serialize(v);
    }
}

/* Python repr() for a string (KeyError str() / single-arg repr; ASCII). */
static char *tms2_repr(const char *s) {
    if (!s) s = "";
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

/* str(exc) for a raise spec: the message already IS str(exc) (KeyError
 * specs carry the repr'd key -- see tms2_make_exc / tms2_sub). */
static char *tms2_exc_str(const json_t *raise) {
    return tms2_strdup(json_get_str(raise, "message", ""));
}

/* Python str.strip() whitespace: ASCII White_Space INCLUDING \x1c-\x1f. */
static bool tms2_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f'
        || c == '\v' || (c >= 0x1c && c <= 0x1f);
}

static char *tms2_strip(const char *s) {
    if (!s) s = "";
    while (*s && tms2_is_ws((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && tms2_is_ws((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - s);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

static char *tms2_lower(const char *s) {
    char *o = tms2_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}

/* -- the one universal seam (fail-closed) -- */
/* Raise-spec messages are str(exc): a scripted KeyError's message is repr'd
 * at materialization (single-arg KeyError str() == repr(arg)). */
static json_t *tms2_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    if (!strcmp(type, "KeyError")) {
        char *r = tms2_repr(msg);
        json_t *e = tms2_err_spec(type, r ? r : msg);
        free(r);
        return e;
    }
    return tms2_err_spec(type, msg);
}

static json_t *tms2_call(tms2_ctx_t *ctx, const char *name, json_t *args) {
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
        ret = tms2_err_spec("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = tms2_err_spec("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = tms2_make_exc(raise);
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
static json_t *tms2_args0(void) { return json_object(); }
static void tms2_aset(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static void tms2_asets(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void tms2_asetb(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void tms2_asetn(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }

/* _err(rid, code, msg) / _ok(rid, payload) -- server-global envelope seams. */
static json_t *tms2_err(tms2_ctx_t *ctx, const json_t *rid, int code, const char *msg) {
    json_t *a = tms2_args0();
    tms2_aset(a, "rid", rid);
    tms2_asetn(a, "code", (double)code);
    tms2_asets(a, "message", msg);
    return tms2_call(ctx, "err", a);
}

static json_t *tms2_ok(tms2_ctx_t *ctx, const json_t *rid, json_t *payload) {
    json_t *a = tms2_args0();
    tms2_aset(a, "rid", rid);
    json_set(a, "payload", payload ? payload : json_null());
    return tms2_call(ctx, "ok", a);
}

/* -- caller-dict state mirrors -- */
static json_t *tms2_state_key(tms2_ctx_t *ctx, const char *key) {
    if (!json_has(ctx->state, key)) json_set(ctx->state, key, json_object());
    return json_obj_get(ctx->state, key);
}

/* -- attribute access on SimpleNamespace doubles -- */
/* Returns a BORROWED value (or the borrowed default when has_default and the
 * key is absent); a missing required attribute sets *raise to the live
 * AttributeError spec. */
static const json_t *tms2_attr(const json_t *obj, const char *key,
                              bool has_default, const json_t *def,
                              json_t **raise) {
    *raise = NULL;
    if (obj && obj->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(obj, key);
        if (v) return v;
        if (has_default) return def;
        char msg[192];
        snprintf(msg, sizeof msg,
                 "'types.SimpleNamespace' object has no attribute '%s'", key);
        *raise = tms2_err_spec("AttributeError", msg);
        return NULL;
    }
    if (has_default) return def;
    char msg[192];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'",
             tms2_type_name(obj), key);
    *raise = tms2_err_spec("AttributeError", msg);
    return NULL;
}

/* dict subscript row[key]: KeyError on a missing key (message is str(exc) ==
 * repr(key)), TypeError on a non-dict receiver. BORROWED. */
static const json_t *tms2_sub(const json_t *obj, const char *key, json_t **raise) {
    *raise = NULL;
    if (obj && obj->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(obj, key);
        if (v) return v;
        char *r = tms2_repr(key);
        *raise = tms2_err_spec("KeyError", r ? r : key);
        free(r);
        return NULL;
    }
    char msg[192];
    if (obj && obj->type == JSON_STRING)
        snprintf(msg, sizeof msg, "string indices must be integers, not 'str'");
    else
        snprintf(msg, sizeof msg, "'%s' object is not subscriptable",
                 tms2_type_name(obj));
    *raise = tms2_err_spec("TypeError", msg);
    return NULL;
}

/* dict.get(key) / .get(key, default) on a plain dict receiver; a non-dict
 * raises the live AttributeError naming 'get'. BORROWED. */
static const json_t *tms2_dict_get(const json_t *obj, const char *key,
                                  bool has_default, const json_t *def,
                                  json_t **raise) {
    *raise = NULL;
    if (obj && obj->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(obj, key);
        if (v) return v;
        return has_default ? def : NULL;
    }
    char msg[192];
    snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'",
             tms2_type_name(obj));
    *raise = tms2_err_spec("AttributeError", msg);
    return NULL;
}

/* `with <non-dict>:` -- the py3.14 context-manager protocol TypeError. */
static json_t *tms2_cm_check(const json_t *v) {
    if (v && v->type == JSON_OBJECT) return NULL;
    char msg[192];
    snprintf(msg, sizeof msg,
             "'%s' object does not support the context manager protocol "
             "(missed __exit__ method)", tms2_type_name(v));
    return tms2_err_spec("TypeError", msg);
}

/* list(x) / for x in <v> iteration gate (fixture domain: list or null). */
static json_t *tms2_iter_check(const json_t *v) {
    if (v && v->type == JSON_ARRAY) return NULL;
    char msg[192];
    snprintf(msg, sizeof msg, "'%s' object is not iterable", tms2_type_name(v));
    return tms2_err_spec("TypeError", msg);
}

/* Unpack v into n OWNED items (lists and strings are iterable). Returns a
 * raise spec on failure (out untouched), NULL on success. */
static json_t *tms2_unpack(const json_t *v, size_t n, json_t **out) {
    size_t len;
    bool is_str = false;
    if (v && v->type == JSON_ARRAY) len = v->c.count;
    else if (v && v->type == JSON_STRING) { len = strlen(v->str_val ? v->str_val : ""); is_str = true; }
    else {
        char msg[192];
        snprintf(msg, sizeof msg, "cannot unpack non-iterable %s object",
                 tms2_type_name(v));
        return tms2_err_spec("TypeError", msg);
    }
    if (len != n) {
        char msg[192];
        snprintf(msg, sizeof msg, "%s values to unpack (expected %zu, got %zu)",
                 len > n ? "too many" : "not enough", n, len);
        return tms2_err_spec("ValueError", msg);
    }
    for (size_t i = 0; i < n; i++) {
        if (is_str) {
            char c[2] = { v->str_val[i], '\0' };
            out[i] = json_string(c);
        } else {
            out[i] = json_copy(v->c.items[i]);
        }
    }
    return NULL;
}

/* {**a, **b}: a must be a dict; b NULL means {}. Returns an owned merge. */
static json_t *tms2_splat_merge(const json_t *a, const json_t *b, json_t **raise) {
    *raise = NULL;
    if (!a || a->type != JSON_OBJECT) {
        char msg[192];
        snprintf(msg, sizeof msg, "'%s' object is not a mapping", tms2_type_name(a));
        *raise = tms2_err_spec("TypeError", msg);
        return NULL;
    }
    json_t *out = json_copy(a);
    if (b && b->type == JSON_OBJECT) {
        for (size_t i = 0; i < b->c.count; i++)
            json_set(out, b->c.keys[i], json_copy(b->c.items[i]));
    }
    return out;
}

/* a | b (dict union): a must be a dict. Returns an owned merge. */
static json_t *tms2_dict_union(const json_t *a, const json_t *b, json_t **raise) {
    *raise = NULL;
    if (!a || a->type != JSON_OBJECT) {
        char msg[192];
        snprintf(msg, sizeof msg,
                 "unsupported operand type(s) for |: '%s' and 'dict'",
                 tms2_type_name(a));
        *raise = tms2_err_spec("TypeError", msg);
        return NULL;
    }
    json_t *out = json_copy(a);
    if (b && b->type == JSON_OBJECT) {
        for (size_t i = 0; i < b->c.count; i++)
            json_set(out, b->c.keys[i], json_copy(b->c.items[i]));
    }
    return out;
}

/* -- claimed-sibling seams -- */
/* _str_param(params, key) -- default "" (every live call site). */
static json_t *tms2_str_param(tms2_ctx_t *ctx, const json_t *params, const char *key) {
    json_t *a = tms2_args0();
    tms2_aset(a, "params", params);
    tms2_asets(a, "key", key);
    tms2_asets(a, "default", "");
    return tms2_call(ctx, "str_param", a);
}

/* _denied_source(row) -- claimed by port_methods_session.c. */
static json_t *tms2_denied_source(tms2_ctx_t *ctx, const json_t *row) {
    json_t *a = tms2_args0();
    tms2_aset(a, "row", row);
    return tms2_call(ctx, "denied_source", a);
}

/* _display_count_field(persistence, sid) -- claimed by tui_methods_flow.c;
 * the persistence handle is omitted from the projection. */
static json_t *tms2_display_count_field(tms2_ctx_t *ctx, const json_t *sid) {
    json_t *a = tms2_args0();
    tms2_aset(a, "session_id", sid);
    return tms2_call(ctx, "display_count_field", a);
}

/* -- CM enter: fire the seam, project the entered handle marker -- */
/* Returns an OWNED entered marker dict, or NULL when the enter value is None
 * (cm_enter_none). *raise carries a seam raise or the with-site TypeError. */
static json_t *tms2_cm_enter(tms2_ctx_t *ctx, const char *seam, json_t *args,
                            json_t **raise) {
    *raise = NULL;
    json_t *r = tms2_call(ctx, seam, args);
    if (tms2_is_raise(r)) { *raise = r; return NULL; }
    if (r && r->type == JSON_OBJECT) {
        if (json_get_bool(r, "cm_enter_none", false)) { json_free(r); return NULL; }
        return r;
    }
    *raise = tms2_cm_check(r);
    json_free(r);
    return NULL;
}

/* persistence.sessions / persistence.lineage sub-object presence gate. */
static json_t *tms2_persist_sub(const json_t *p, const char *sub) {
    if (p && p->type == JSON_OBJECT && json_obj_get(p, sub)) return NULL;
    char msg[192];
    if (p && p->type == JSON_OBJECT)
        snprintf(msg, sizeof msg,
                 "'types.SimpleNamespace' object has no attribute '%s'", sub);
    else
        snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'",
                 tms2_type_name(p), sub);
    return tms2_err_spec("AttributeError", msg);
}

/* -- persistence method seams (sub-object gated) -- */
static json_t *tms2_sessions_get(tms2_ctx_t *ctx, const json_t *p, const json_t *sid) {
    json_t *bad = tms2_persist_sub(p, "sessions");
    if (bad) return bad;
    json_t *a = tms2_args0();
    tms2_aset(a, "session_id", sid);
    return tms2_call(ctx, "sessions_get", a);
}

static json_t *tms2_get_by_title(tms2_ctx_t *ctx, const json_t *p, const char *title) {
    json_t *bad = tms2_persist_sub(p, "sessions");
    if (bad) return bad;
    json_t *a = tms2_args0();
    tms2_asets(a, "title", title);
    return tms2_call(ctx, "get_by_title", a);
}

static json_t *tms2_lineage_tip(tms2_ctx_t *ctx, const json_t *p, const json_t *sid) {
    json_t *bad = tms2_persist_sub(p, "lineage");
    if (bad) return bad;
    json_t *a = tms2_args0();
    tms2_aset(a, "session_id", sid);
    return tms2_call(ctx, "lineage_tip", a);
}

/* record.as_session_row() -- the _row_of projection seam. */
static json_t *tms2_as_session_row(tms2_ctx_t *ctx, const json_t *record) {
    if (!record || record->type != JSON_OBJECT) {
        char msg[192];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'as_session_row'",
                 tms2_type_name(record));
        return tms2_err_spec("AttributeError", msg);
    }
    return tms2_call(ctx, "as_session_row", tms2_args0());
}

/* -- base64 (standard alphabet, padded) -- */
static char *tms2_b64_encode(const char *data) {
    static const char enc[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t n = strlen(data);
    size_t olen = 4 * ((n + 2) / 3);
    char *o = malloc(olen + 1);
    if (!o) return NULL;
    size_t k = 0;
    for (size_t i = 0; i < n; i += 3) {
        unsigned int b0 = (unsigned char)data[i];
        unsigned int b1 = (i + 1 < n) ? (unsigned char)data[i + 1] : 0;
        unsigned int b2 = (i + 2 < n) ? (unsigned char)data[i + 2] : 0;
        o[k++] = enc[b0 >> 2];
        o[k++] = enc[((b0 & 3) << 4) | (b1 >> 4)];
        o[k++] = (i + 1 < n) ? enc[((b1 & 15) << 2) | (b2 >> 6)] : '=';
        o[k++] = (i + 2 < n) ? enc[b2 & 63] : '=';
    }
    o[k] = '\0';
    return o;
}

/* -- strict Python json.loads grammar pre-validator -- */
/* libjson's parser is lenient (leading zeros, bad escapes, ...); Python
 * json.loads is strict. Validate the grammar FIRST, then parse. Fixture
 * domain: ASCII, no lone-surrogate \u escapes, no NaN/Infinity. */
typedef struct tms2_jv { const char *s; size_t i, n; } tms2_jv_t;

static void tms2_jv_ws(tms2_jv_t *p) {
    while (p->i < p->n) {
        char c = p->s[p->i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') p->i++;
        else break;
    }
}

static bool tms2_jv_value(tms2_jv_t *p);

static bool tms2_jv_string(tms2_jv_t *p) {
    if (p->i >= p->n || p->s[p->i] != '"') return false;
    p->i++;
    while (p->i < p->n) {
        unsigned char c = (unsigned char)p->s[p->i++];
        if (c == '"') return true;
        if (c == '\\') {
            if (p->i >= p->n) return false;
            char e = p->s[p->i++];
            if (e == '"' || e == '\\' || e == '/' || e == 'b' || e == 'f'
                || e == 'n' || e == 'r' || e == 't') continue;
            if (e == 'u') {
                for (int k = 0; k < 4; k++) {
                    if (p->i >= p->n || !isxdigit((unsigned char)p->s[p->i])) return false;
                    p->i++;
                }
                continue;
            }
            return false;
        }
        if (c < 0x20) return false;
    }
    return false;
}

static bool tms2_jv_number(tms2_jv_t *p) {
    size_t start = p->i;
    if (p->i < p->n && p->s[p->i] == '-') p->i++;
    if (p->i >= p->n) return false;
    if (p->s[p->i] == '0') {
        p->i++;
        if (p->i < p->n && isdigit((unsigned char)p->s[p->i])) return false;
    } else if (isdigit((unsigned char)p->s[p->i])) {
        while (p->i < p->n && isdigit((unsigned char)p->s[p->i])) p->i++;
    } else return false;
    if (p->i < p->n && p->s[p->i] == '.') {
        p->i++;
        if (p->i >= p->n || !isdigit((unsigned char)p->s[p->i])) return false;
        while (p->i < p->n && isdigit((unsigned char)p->s[p->i])) p->i++;
    }
    if (p->i < p->n && (p->s[p->i] == 'e' || p->s[p->i] == 'E')) {
        p->i++;
        if (p->i < p->n && (p->s[p->i] == '+' || p->s[p->i] == '-')) p->i++;
        if (p->i >= p->n || !isdigit((unsigned char)p->s[p->i])) return false;
        while (p->i < p->n && isdigit((unsigned char)p->s[p->i])) p->i++;
    }
    return p->i > start;
}

static bool tms2_jv_lit(tms2_jv_t *p, const char *lit) {
    size_t n = strlen(lit);
    if (p->i + n > p->n || memcmp(p->s + p->i, lit, n) != 0) return false;
    p->i += n;
    return true;
}

static bool tms2_jv_value(tms2_jv_t *p) {
    tms2_jv_ws(p);
    if (p->i >= p->n) return false;
    char c = p->s[p->i];
    if (c == '"') return tms2_jv_string(p);
    if (c == '{') {
        p->i++;
        tms2_jv_ws(p);
        if (p->i < p->n && p->s[p->i] == '}') { p->i++; return true; }
        for (;;) {
            tms2_jv_ws(p);
            if (!tms2_jv_string(p)) return false;
            tms2_jv_ws(p);
            if (p->i >= p->n || p->s[p->i] != ':') return false;
            p->i++;
            if (!tms2_jv_value(p)) return false;
            tms2_jv_ws(p);
            if (p->i < p->n && p->s[p->i] == ',') { p->i++; continue; }
            if (p->i < p->n && p->s[p->i] == '}') { p->i++; return true; }
            return false;
        }
    }
    if (c == '[') {
        p->i++;
        tms2_jv_ws(p);
        if (p->i < p->n && p->s[p->i] == ']') { p->i++; return true; }
        for (;;) {
            if (!tms2_jv_value(p)) return false;
            tms2_jv_ws(p);
            if (p->i < p->n && p->s[p->i] == ',') { p->i++; continue; }
            if (p->i < p->n && p->s[p->i] == ']') { p->i++; return true; }
            return false;
        }
    }
    if (c == 't') return tms2_jv_lit(p, "true");
    if (c == 'f') return tms2_jv_lit(p, "false");
    if (c == 'n') return tms2_jv_lit(p, "null");
    if (c == '-' || isdigit((unsigned char)c)) return tms2_jv_number(p);
    return false;
}

/* json.loads(text): 0 on success (*out owned), -1 on JSONDecodeError. */
static int tms2_json_loads(const char *text, json_t **out) {
    *out = NULL;
    if (!text) return -1;
    tms2_jv_t p = { text, 0, strlen(text) };
    if (!tms2_jv_value(&p)) return -1;
    tms2_jv_ws(&p);
    if (p.i != p.n) return -1;
    json_t *v = json_parse(text, NULL);
    if (!v) return -1;
    *out = v;
    return 0;
}/* ======================= ported handler plumbing ======================= */

/* _session_arg's generated handler: session, err = resolve(params, rid);
 * return err or fn(rid, params, session). The resolve lambda is the
 * "resolve" seam; fn is the "session_fn" callback seam. */
/* PoP: tms2_session_arg @ tui_gateway/methods_session.py:_session_arg */
static json_t *tms2_session_arg(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    json_t *ra = tms2_args0();
    tms2_aset(ra, "params", params);
    tms2_aset(ra, "rid", rid);
    json_t *pair = tms2_call(ctx, "resolve", ra);
    if (tms2_is_raise(pair)) return pair;
    json_t *u[2];
    json_t *bad = tms2_unpack(pair, 2, u);
    json_free(pair);
    if (bad) return bad;
    if (tms2_truthy(u[1])) { json_free(u[0]); return u[1]; }
    json_free(u[1]);
    json_t *fa = tms2_args0();
    tms2_aset(fa, "rid", rid);
    tms2_aset(fa, "params", params);
    tms2_aset(fa, "session", u[0]);
    json_t *r = tms2_call(ctx, "session_fn", fa);
    json_free(u[0]);
    return r;
}

/* _session_method's generated handler: live picks _sess (waits for the agent
 * build) over _sess_nowait; the rest is _session_arg's flow. name is the
 * decoration-time registry key (identity decorator; no event). */
/* PoP: tms2_session_method @ tui_gateway/methods_session.py:_session_method */
static json_t *tms2_session_method(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    bool live = json_get_bool(args, "live", false);
    json_t *ra = tms2_args0();
    tms2_aset(ra, "params", params);
    tms2_aset(ra, "rid", rid);
    json_t *pair = tms2_call(ctx, live ? "sess" : "sess_nowait", ra);
    if (tms2_is_raise(pair)) return pair;
    json_t *u[2];
    json_t *bad = tms2_unpack(pair, 2, u);
    json_free(pair);
    if (bad) return bad;
    if (tms2_truthy(u[1])) { json_free(u[0]); return u[1]; }
    json_free(u[1]);
    json_t *fa = tms2_args0();
    tms2_aset(fa, "rid", rid);
    tms2_aset(fa, "params", params);
    tms2_aset(fa, "session", u[0]);
    json_t *r = tms2_call(ctx, "session_fn", fa);
    json_free(u[0]);
    return r;
}

/* _with_db's generated handler: (scoped ? _with_session first) then
 * `with db_cm as db, _persistence_for(db) as persistence:`; None persistence
 * -> _db_unavailable_error(rid, code=code); else fn(rid, params, *session,
 * persistence). */
/* PoP: tms2_with_db @ tui_gateway/methods_session.py:_with_db */
static json_t *tms2_with_db(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    bool scoped = json_get_bool(args, "session_scoped", false);
    int code = (int)json_get_num(args, "code", 0);
    json_t *session = NULL;
    if (scoped) {
        json_t *ra = tms2_args0();
        tms2_aset(ra, "params", params);
        tms2_aset(ra, "rid", rid);
        json_t *pair = tms2_call(ctx, "sess_nowait", ra);
        if (tms2_is_raise(pair)) return pair;
        json_t *u[2];
        json_t *bad = tms2_unpack(pair, 2, u);
        json_free(pair);
        if (bad) return bad;
        if (tms2_truthy(u[1])) { json_free(u[0]); return u[1]; }
        json_free(u[1]);
        session = u[0];
    }
    json_t *da = tms2_args0();
    if (scoped) tms2_aset(da, "session", session);
    else tms2_aset(da, "params", params);
    json_t *raise = NULL;
    json_t *db = tms2_cm_enter(ctx, scoped ? "session_db" : "profile_db", da, &raise);
    if (raise) { json_free(session); return raise; }
    json_t *pa = tms2_args0();
    tms2_aset(pa, "db", db);
    json_t *p = tms2_cm_enter(ctx, "persistence_for", pa, &raise);
    if (raise) { json_free(db); json_free(session); return raise; }
    json_t *ret;
    if (!p) {
        json_t *ua = tms2_args0();
        tms2_aset(ua, "rid", rid);
        tms2_asetn(ua, "code", (double)code);
        ret = tms2_call(ctx, "db_unavailable_error", ua);
    } else {
        json_t *fa = tms2_args0();
        tms2_aset(fa, "rid", rid);
        tms2_aset(fa, "params", params);
        if (scoped) tms2_aset(fa, "session", session);
        tms2_aset(fa, "persistence", p);
        ret = tms2_call(ctx, "db_fn", fa);
    }
    json_free(db);
    json_free(p);
    json_free(session);
    return ret;
}

/* ======================= session mint / build helpers ======================= */

/* (uuid.uuid4().hex[:8], _resolve_session_source(_str_param(params, "source")
 * or None)) */
/* PoP: tms2_new_runtime_ids @ tui_gateway/methods_session.py:_new_runtime_ids */
static json_t *tms2_new_runtime_ids(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *params = json_obj_get(args, "params");
    json_t *hex = tms2_call(ctx, "uuid4_hex", tms2_args0());
    if (tms2_is_raise(hex)) return hex;
    const char *hs = tms2_str(hex);
    if (!hs) {
        char msg[192];
        snprintf(msg, sizeof msg, "'%s' object is not subscriptable",
                 tms2_type_name(hex));
        json_free(hex);
        return tms2_err_spec("TypeError", msg);
    }
    char sid8[9];
    size_t n = strlen(hs);
    if (n > 8) n = 8;
    memcpy(sid8, hs, n);
    sid8[n] = '\0';
    json_free(hex);
    json_t *sp = tms2_str_param(ctx, params, "source");
    if (tms2_is_raise(sp)) return sp;
    json_t *source = tms2_truthy(sp) ? sp : json_null();
    if (source != sp) json_free(sp);
    json_t *sa = tms2_args0();
    tms2_aset(sa, "source", source);
    json_t *rs = tms2_call(ctx, "resolve_session_source", sa);
    json_free(source);
    if (tms2_is_raise(rs)) return rs;
    json_t *out = json_array();
    json_append(out, json_string(sid8));
    json_append(out, rs);
    return out;
}

/* _session_profile_runtime_scope({"profile_home": str(profile_home) if
 * profile_home else None}) */
/* PoP: tms2_profile_build_scope @ tui_gateway/methods_session.py:_profile_build_scope */
static json_t *tms2_profile_build_scope(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *home = json_obj_get(args, "profile_home");
    char *ph = tms2_truthy(home) ? tms2_pystr(home) : NULL;
    json_t *a = tms2_args0();
    tms2_asets(a, "profile_home", ph);
    free(ph);
    return tms2_call(ctx, "session_profile_runtime_scope", a);
}

/* tokens = _set_session_context(key, cwd=kwargs.get("cwd_override"));
 * try: return _make_agent(sid, key, session_id=key, **kwargs)
 * finally: _clear_session_context(tokens) */
/* PoP: tms2_make_agent_in_context @ tui_gateway/methods_session.py:_make_agent_in_context */
static json_t *tms2_make_agent_in_context(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *sid = json_obj_get(args, "sid");
    const json_t *key = json_obj_get(args, "key");
    const json_t *kwargs = json_obj_get(args, "kwargs");
    if (kwargs && kwargs->type != JSON_OBJECT) kwargs = NULL;
    const json_t *cwd = kwargs ? json_obj_get(kwargs, "cwd_override") : NULL;
    json_t *ca = tms2_args0();
    tms2_aset(ca, "key", key);
    tms2_aset(ca, "cwd", cwd);
    json_t *tokens = tms2_call(ctx, "set_session_context", ca);
    if (tms2_is_raise(tokens)) return tokens;
    json_t *ma = tms2_args0();
    tms2_aset(ma, "sid", sid);
    tms2_aset(ma, "key", key);
    tms2_aset(ma, "session_id", key);
    tms2_aset(ma, "kwargs", kwargs);
    json_t *r = tms2_call(ctx, "make_agent", ma);
    /* finally: the clear fires on EVERY path; its own raise replaces r. */
    json_t *cla = tms2_args0();
    tms2_aset(cla, "tokens", tokens);
    json_t *c = tms2_call(ctx, "clear_session_context", cla);
    json_free(tokens);
    if (tms2_is_raise(c)) { json_free(r); return c; }
    json_free(c);
    return r;
}

/* session.info after a cwd change: the full agent view, or the lazy shape. */
/* PoP: tms2_cwd_info @ tui_gateway/methods_session.py:_cwd_info */
static json_t *tms2_cwd_info(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *session = json_obj_get(args, "session");
    const json_t *cwd = json_obj_get(args, "cwd");
    const json_t *branch = json_obj_get(args, "branch");
    json_t *raise = NULL;
    const json_t *agent = tms2_dict_get(session, "agent", false, NULL, &raise);
    if (raise) return raise;
    if (agent && agent->type != JSON_NULL) {
        json_t *a = tms2_args0();
        tms2_aset(a, "agent", agent);
        tms2_aset(a, "session", session);
        return tms2_call(ctx, "session_info", a);
    }
    json_t *out = json_object();
    json_set(out, "cwd", cwd ? json_copy(cwd) : json_null());
    if (branch && branch->type != JSON_NULL) {
        json_set(out, "branch", json_copy(branch));
    } else {
        json_t *ba = tms2_args0();
        tms2_aset(ba, "cwd", cwd);
        json_t *b = tms2_call(ctx, "git_branch", ba);
        if (tms2_is_raise(b)) { json_free(out); return b; }
        json_set(out, "branch", b);
    }
    json_t *pa = tms2_args0();
    tms2_aset(pa, "cwd", cwd);
    json_t *pj = tms2_call(ctx, "project_info_for_cwd", pa);
    if (tms2_is_raise(pj)) { json_free(out); return pj; }
    json_set(out, "project", pj);
    json_set(out, "lazy", json_bool(true));
    /* DESKTOP_BACKEND_CONTRACT (tui_gateway/server.py) */
    json_set(out, "desktop_contract", json_number(8));
    return out;
}

/* ======================= listing rows / summaries ======================= */

/* Compact session.list row; tip_row/resolved_id from the compression tip;
 * persistence adds _display_count_field (claimed sibling seam). */
/* PoP: tms2_session_row_summary @ tui_gateway/methods_session.py:_session_row_summary */
static json_t *tms2_session_row_summary(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *row = json_obj_get(args, "row");
    const json_t *tip_row = json_obj_get(args, "tip_row");
    const json_t *resolved_id = json_obj_get(args, "resolved_id");
    const json_t *persistence = json_obj_get(args, "persistence");
    json_t *raise = NULL;
    const json_t *id = tms2_sub(row, "id", &raise);
    if (raise) return raise;
    const json_t *tip = tms2_truthy(tip_row) ? tip_row : row;
    bool has_resolved = resolved_id && resolved_id->type != JSON_NULL;
    json_t *out = json_object();
    json_set(out, "id", json_copy(id));
    if (has_resolved) json_set(out, "resolved_id", json_copy(resolved_id));
    const json_t *title = tms2_dict_get(row, "title", false, NULL, &raise);
    if (raise) { json_free(out); return raise; }
    json_set(out, "title", tms2_truthy(title) ? json_copy(title) : json_string(""));
    const json_t *preview = tms2_dict_get(tip, "preview", false, NULL, &raise);
    if (raise) { json_free(out); return raise; }
    json_set(out, "preview", tms2_truthy(preview) ? json_copy(preview) : json_string(""));
    const json_t *started = json_obj_get(row, "started_at");
    json_set(out, "started_at", tms2_truthy(started) ? json_copy(started) : json_number(0));
    const json_t *mcount = json_obj_get(tip, "message_count");
    json_set(out, "message_count", tms2_truthy(mcount) ? json_copy(mcount) : json_number(0));
    const json_t *source = json_obj_get(row, "source");
    json_set(out, "source", tms2_truthy(source) ? json_copy(source) : json_string(""));
    const json_t *root = json_obj_get(row, "_lineage_root_id");
    json_set(out, "_lineage_root_id", root ? json_copy(root) : json_null());
    if (persistence && persistence->type == JSON_OBJECT) {
        const json_t *sid = has_resolved ? resolved_id : id;
        json_t *dcf = tms2_display_count_field(ctx, sid);
        if (tms2_is_raise(dcf)) { json_free(out); return dcf; }
        if (!dcf || dcf->type != JSON_OBJECT) {
            char msg[192];
            snprintf(msg, sizeof msg, "'%s' object is not a mapping",
                     tms2_type_name(dcf));
            json_free(dcf);
            json_free(out);
            return tms2_err_spec("TypeError", msg);
        }
        for (size_t i = 0; i < dcf->c.count; i++)
            json_set(out, dcf->c.keys[i], json_copy(dcf->c.items[i]));
        json_free(dcf);
    }
    return out;
}

/* None if record is None else record.as_session_row(). */
/* PoP: tms2_row_of @ tui_gateway/methods_session.py:_row_of */
static json_t *tms2_row_of(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *record = json_obj_get(args, "record");
    if (!record || record->type == JSON_NULL) return json_null();
    return tms2_as_session_row(ctx, record);
}

/* Human-facing listing rows (most recent first), deny-list applied. */
/* PoP: tms2_listing_rows @ tui_gateway/methods_session.py:_listing_rows */
static json_t *tms2_listing_rows(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *limit = json_obj_get(args, "limit");
    const json_t *kwargs = json_obj_get(args, "kwargs");
    json_t *a = tms2_args0();
    tms2_asets(a, "order", "LAST_ACTIVE");
    tms2_asetn(a, "limit", limit ? json_get_num(args, "limit", 0) : 0);
    tms2_asetb(a, "compact_rows", true);
    if (kwargs && kwargs->type == JSON_OBJECT) {
        for (size_t i = 0; i < kwargs->c.count; i++)
            json_set(a, kwargs->c.keys[i], json_copy(kwargs->c.items[i]));
    }
    json_t *entries = tms2_call(ctx, "list_sessions", a);
    if (tms2_is_raise(entries)) return entries;
    json_t *bad = tms2_iter_check(entries);
    if (bad) { json_free(entries); return bad; }
    json_t *out = json_array();
    for (size_t i = 0; i < entries->c.count; i++) {
        const json_t *entry = entries->c.items[i];
        if (!entry || entry->type != JSON_OBJECT) {
            char msg[192];
            snprintf(msg, sizeof msg, "'%s' object has no attribute 'as_list_row'",
                     tms2_type_name(entry));
            json_free(out);
            json_free(entries);
            return tms2_err_spec("AttributeError", msg);
        }
        json_t *row = tms2_call(ctx, "as_list_row", tms2_args0());
        if (tms2_is_raise(row)) { json_free(out); json_free(entries); return row; }
        json_t *denied = tms2_denied_source(ctx, row);
        if (tms2_is_raise(denied)) {
            json_free(row); json_free(out); json_free(entries);
            return denied;
        }
        if (!tms2_truthy(denied)) json_append(out, row);
        else json_free(row);
        json_free(denied);
    }
    json_free(entries);
    return out;
}

/* (list(_sessions.items()), None) under the silent lock, or
 * (None, _err(rid, 5036, ...)) -- fail CLOSED. */
/* PoP: tms2_snapshot_sessions @ tui_gateway/methods_session.py:_snapshot_sessions */
static json_t *tms2_snapshot_sessions(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    json_t *items = tms2_call(ctx, "sessions_items", tms2_args0());
    json_t *exc = NULL;
    if (tms2_is_raise(items)) {
        if (tms2_is_base_exc(tms2_raise_kind(items))) return items;
        exc = items;
        items = NULL;
    } else {
        json_t *bad = tms2_iter_check(items);
        if (bad) { exc = bad; json_free(items); items = NULL; }
    }
    if (exc) {
        char *es = tms2_exc_str(exc);
        json_free(exc);
        size_t mn = strlen(es) + 48;
        char *msg = malloc(mn);
        if (msg) snprintf(msg, mn, "could not enumerate active sessions: %s", es);
        free(es);
        json_t *err = tms2_err(ctx, rid, 5036, msg ? msg : "");
        free(msg);
        if (tms2_is_raise(err)) return err;
        json_t *out = json_array();
        json_append(out, json_null());
        json_append(out, err);
        return out;
    }
    json_t *out = json_array();
    json_append(out, items);
    json_append(out, json_null());
    return out;
}

/* ======================= pet helpers ======================= */

/* display.pet config block, {} when config is unreadable. */
/* PoP: tms2_pet_display_cfg @ tui_gateway/methods_session.py:_pet_display_cfg */
static json_t *tms2_pet_display_cfg(tms2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *cfg = tms2_call(ctx, "load_config", tms2_args0());
    if (tms2_is_raise(cfg)) {
        if (tms2_is_base_exc(tms2_raise_kind(cfg))) return cfg;
        json_free(cfg);
        return json_object();
    }
    json_t *raise = NULL;
    const json_t *disp = tms2_dict_get(cfg, "display", false, NULL, &raise);
    if (raise) { json_free(cfg); json_free(raise); return json_object(); }
    const json_t *pet = (disp && disp->type == JSON_OBJECT)
                        ? json_obj_get(disp, "pet") : NULL;
    json_t *out = (pet && pet->type == JSON_OBJECT) ? json_copy(pet) : json_object();
    json_free(cfg);
    return out;
}

/* Best-effort progress emit: a transport hiccup must never abort generation. */
/* PoP: tms2_pet_emit @ tui_gateway/methods_session.py:_pet_emit */
static json_t *tms2_pet_emit(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *event = json_obj_get(args, "event");
    const json_t *payload = json_obj_get(args, "payload");
    const char *what = json_get_str(args, "what", "");
    json_t *a = tms2_args0();
    tms2_aset(a, "event", event);
    tms2_asets(a, "session_id", "");
    tms2_aset(a, "payload", payload);
    json_t *r = tms2_call(ctx, "emit", a);
    if (tms2_is_raise(r)) {
        if (tms2_is_base_exc(tms2_raise_kind(r))) return r;
        char *es = tms2_exc_str(r);
        json_free(r);
        size_t mn = strlen(what) + strlen(es) + 24;
        char *msg = malloc(mn);
        if (msg) snprintf(msg, mn, "%s emit failed: %s", what, es);
        free(es);
        json_t *la = tms2_args0();
        tms2_asets(la, "msg", msg ? msg : "");
        json_t *lg = tms2_call(ctx, "logger_debug", la);
        free(msg);
        if (tms2_is_raise(lg)) return lg;
        json_free(lg);
    } else {
        json_free(r);
    }
    return json_null();
}

/* Release the cancel arm for token and return _err. */
/* PoP: tms2_pet_gen_abort @ tui_gateway/methods_session.py:_pet_gen_abort */
static json_t *tms2_pet_gen_abort(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *token = json_obj_get(args, "token");
    int code = (int)json_get_num(args, "code", 0);
    const char *message = json_get_str(args, "message", "");
    json_t *ta = tms2_args0();
    tms2_aset(ta, "token", token);
    json_t *r = tms2_call(ctx, "pet_cancel_release", ta);
    if (tms2_is_raise(r)) return r;
    json_free(r);
    return tms2_err(ctx, rid, code, message);
}

/* _pet_method's generated handler: slug gate (4004), fn call, except ->
 * logger.debug + fail_open (payload or params->payload) or _err(5031).
 * name/scoped are decoration-time only (identity registry; no events). */
/* PoP: tms2_pet_method @ tui_gateway/methods_session.py:_pet_method */
static json_t *tms2_pet_method(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    const char *name = json_get_str(args, "name", "");
    bool slug = json_get_bool(args, "slug", false);
    const json_t *fail_open = json_obj_get(args, "fail_open");
    json_t *value = NULL;
    json_t *r = NULL;
    bool raised = false;
    if (slug) {
        value = tms2_str_param(ctx, params, "slug");
        if (tms2_is_raise(value)) { r = value; value = NULL; raised = true; }
        else if (!tms2_truthy(value)) {
            json_free(value);
            return tms2_err(ctx, rid, 4004, "missing slug");
        }
    }
    if (!raised) {
        json_t *fa = tms2_args0();
        tms2_aset(fa, "rid", rid);
        tms2_aset(fa, "params", params);
        if (slug) tms2_aset(fa, "slug", value);
        r = tms2_call(ctx, "pet_fn", fa);
        if (slug) { json_free(value); value = NULL; }
        if (!tms2_is_raise(r)) return r;
        raised = true;
    }
    /* except Exception as exc */
    if (tms2_is_base_exc(tms2_raise_kind(r))) return r;
    char *es = tms2_exc_str(r);
    json_free(r);
    size_t mn = strlen(name) + strlen(es) + 16;
    char *msg = malloc(mn);
    if (msg) snprintf(msg, mn, "%s failed: %s", name, es);
    free(es);
    json_t *la = tms2_args0();
    tms2_asets(la, "msg", msg ? msg : "");
    json_t *lg = tms2_call(ctx, "logger_debug", la);
    if (tms2_is_raise(lg)) { free(msg); return lg; }
    json_free(lg);
    if (fail_open && fail_open->type != JSON_NULL) {
        json_t *payload;
        if (fail_open->type == JSON_OBJECT
            && json_get_bool(fail_open, "__callable__", false)) {
            json_t *foa = tms2_args0();
            tms2_aset(foa, "params", params);
            payload = tms2_call(ctx, "fail_open_fn", foa);
            if (tms2_is_raise(payload)) { free(msg); return payload; }
        } else if (fail_open->type == JSON_OBJECT) {
            payload = json_copy(fail_open);   /* dict(fail_open) */
        } else {
            /* outside the fixture domain (null/dict/callable only) */
            char m2[160];
            snprintf(m2, sizeof m2, "'%s' object is not iterable",
                     tms2_type_name(fail_open));
            free(msg);
            return tms2_err_spec("TypeError", m2);
        }
        free(msg);
        return tms2_ok(ctx, rid, payload);
    }
    json_t *er = tms2_err(ctx, rid, 5031, msg ? msg : "");
    free(msg);
    return er;
}

/* (pet, scale) when enabled and the pet exists, else None. */
/* PoP: tms2_active_pet @ tui_gateway/methods_session.py:_active_pet */
static json_t *tms2_active_pet(tms2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *sel = tms2_call(ctx, "pet_active_selection", tms2_args0());
    if (tms2_is_raise(sel)) return sel;
    json_t *u[3];
    json_t *bad = tms2_unpack(sel, 3, u);
    json_free(sel);
    if (bad) return bad;
    json_t *out = NULL;
    if (!tms2_truthy(u[0])) {
        out = json_null();
    } else if (!u[1] || u[1]->type == JSON_NULL) {
        out = json_null();
    } else {
        json_t *raise = NULL;
        const json_t *exists = tms2_attr(u[1], "exists", false, NULL, &raise);
        if (raise) {
            json_free(u[0]); json_free(u[1]); json_free(u[2]);
            return raise;
        }
        if (!tms2_truthy(exists)) {
            out = json_null();
        } else {
            out = json_array();
            json_append(out, json_copy(u[1]));
            json_append(out, u[2]);
            u[2] = NULL;
        }
    }
    json_free(u[0]); json_free(u[1]); json_free(u[2]);
    return out;
}

/* base64.standard_b64encode(data).decode("ascii") -- ASCII bytes domain. */
/* PoP: tms2_b64 @ tui_gateway/methods_session.py:_b64 */
static json_t *tms2_b64(tms2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *data = json_obj_get(args, "data");
    const char *s = tms2_str(data);
    if (!s) {
        char msg[192];
        snprintf(msg, sizeof msg, "a bytes-like object is required, not '%s'",
                 tms2_type_name(data));
        return tms2_err_spec("TypeError", msg);
    }
    char *enc = tms2_b64_encode(s);
    json_t *out = json_string(enc ? enc : "");
    free(enc);
    return out;
}

/* Best-effort fn(*args); except -> logger.debug. */
/* PoP: tms2_pet_config_followup @ tui_gateway/methods_session.py:_pet_config_followup */
static json_t *tms2_pet_config_followup(tms2_ctx_t *ctx, const json_t *args) {
    const char *what = json_get_str(args, "what", "");
    const json_t *fargs = json_obj_get(args, "args");
    json_t *a = tms2_args0();
    json_set(a, "args", (fargs && fargs->type == JSON_ARRAY) ? json_copy(fargs) : json_array());
    json_t *r = tms2_call(ctx, "followup_fn", a);
    if (tms2_is_raise(r)) {
        if (tms2_is_base_exc(tms2_raise_kind(r))) return r;
        char *es = tms2_exc_str(r);
        json_free(r);
        size_t mn = strlen(what) + strlen(es) + 32;
        char *msg = malloc(mn);
        if (msg) snprintf(msg, mn, "%s config update failed: %s", what, es);
        free(es);
        json_t *la = tms2_args0();
        tms2_asets(la, "msg", msg ? msg : "");
        json_t *lg = tms2_call(ctx, "logger_debug", la);
        free(msg);
        if (tms2_is_raise(lg)) return lg;
        json_free(lg);
    } else {
        json_free(r);
    }
    return json_null();
}

/* Picker-chosen params.provider resolved up front; no pick -> None. */
/* PoP: tms2_pet_pick_provider @ tui_gateway/methods_session.py:_pet_pick_provider */
static json_t *tms2_pet_pick_provider(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *params = json_obj_get(args, "params");
    bool req = json_get_bool(args, "require_references", false);
    json_t *name = tms2_str_param(ctx, params, "provider");
    if (tms2_is_raise(name)) return name;
    if (!tms2_truthy(name)) { json_free(name); return json_null(); }
    json_t *a = tms2_args0();
    tms2_asetb(a, "require_references", req);
    tms2_aset(a, "prefer", name);
    json_t *r = tms2_call(ctx, "resolve_provider", a);
    json_free(name);
    return r;
}/* ======================= billing envelopes ======================= */

/* Deep scalar equality (Python == on the fixture domain; bool==int cross). */
static bool tms2_json_eq(const json_t *a, const json_t *b) {
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
            if (!tms2_json_eq(a->c.items[i], b->c.items[i])) return false;
        return true;
    }
    if (a->type == JSON_OBJECT) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !tms2_json_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}

/* FULL builtin OSError hierarchy (except OSError catches every subclass). */
static bool tms2_is_oserror(const char *kind) {
    static const char *const fam[] = {
        "OSError", "BlockingIOError", "ChildProcessError", "ConnectionError",
        "BrokenPipeError", "ConnectionAbortedError", "ConnectionRefusedError",
        "ConnectionResetError", "FileExistsError", "FileNotFoundError",
        "InterruptedError", "IsADirectoryError", "NotADirectoryError",
        "PermissionError", "ProcessLookupError", "TimeoutError", "WindowsError",
    };
    if (!kind) return false;
    for (size_t i = 0; i < sizeof(fam) / sizeof(fam[0]); i++)
        if (!strcmp(kind, fam[i])) return true;
    return false;
}

/* _billing_call core. fn_mode 0: the "billing_fn" callback seam.
 * fn_mode 1 (_billing_route's lambda): the "route_call" seam, then
 * `result | (extra or {})` INSIDE the try. extra NULL means {}. */
static json_t *tms2_billing_call_core(tms2_ctx_t *ctx, const json_t *rid,
                                     int fn_mode, const json_t *params,
                                     const json_t *key, const json_t *extra) {
    json_t *result = NULL;
    json_t *exc = NULL;
    if (fn_mode == 0) {
        result = tms2_call(ctx, "billing_fn", tms2_args0());
        if (tms2_is_raise(result)) { exc = result; result = NULL; }
    } else {
        json_t *ca = tms2_args0();
        tms2_aset(ca, "params", params);
        tms2_aset(ca, "key", key);
        result = tms2_call(ctx, "route_call", ca);
        if (tms2_is_raise(result)) { exc = result; result = NULL; }
        else {
            json_t *raise = NULL;
            json_t *merged = tms2_dict_union(result, extra, &raise);
            json_free(result);
            result = NULL;
            if (raise) exc = raise;
            else result = merged;
        }
    }
    if (exc) {
        if (tms2_is_base_exc(tms2_raise_kind(exc))) return exc;
        const char *kind = tms2_raise_kind(exc);
        if (kind && !strcmp(kind, "BillingError")) {
            json_t *sa = tms2_args0();
            tms2_aset(sa, "exc", exc);
            json_t *ser = tms2_call(ctx, "serialize_billing_error", sa);
            json_free(exc);
            if (tms2_is_raise(ser)) return ser;
            json_t *raise = NULL;
            json_t *payload = tms2_splat_merge(ser, extra, &raise);
            json_free(ser);
            if (raise) return raise;
            return tms2_ok(ctx, rid, payload);
        }
        char *es = tms2_exc_str(exc);
        json_free(exc);
        json_t *payload = json_object();
        json_set(payload, "ok", json_bool(false));
        json_set_hdr(payload, "error", "error");
        json_set_hdr(payload, "message", es);
        free(es);
        if (extra && extra->type == JSON_OBJECT) {
            for (size_t i = 0; i < extra->c.count; i++)
                json_set(payload, extra->c.keys[i], json_copy(extra->c.items[i]));
        }
        return tms2_ok(ctx, rid, payload);
    }
    return tms2_ok(ctx, rid, result);
}

/* Portal call -> ok; BillingError -> serialized envelope, else generic;
 * extra rides both ERROR envelopes. */
/* PoP: tms2_billing_call @ tui_gateway/methods_session.py:_billing_call */
static json_t *tms2_billing_call(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *extra = json_obj_get(args, "extra");
    return tms2_billing_call_core(ctx, rid, 0, NULL, NULL, extra);
}

/* _ok(rid, {"ok": False, "error": error, "message": message}) */
/* PoP: tms2_billing_invalid @ tui_gateway/methods_session.py:_billing_invalid */
static json_t *tms2_billing_invalid(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const char *message = json_get_str(args, "message", "");
    const char *error = json_get_str(args, "error", "invalid_request");
    json_t *payload = json_object();
    json_set(payload, "ok", json_bool(false));
    json_set_hdr(payload, "error", error);
    json_set_hdr(payload, "message", message);
    return tms2_ok(ctx, rid, payload);
}

/* {"ok": True, "message": result.get("message"), "payload": result} */
/* PoP: tms2_billing_pending_change @ tui_gateway/methods_session.py:_billing_pending_change */
static json_t *tms2_billing_pending_change(tms2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *result = json_obj_get(args, "result");
    json_t *raise = NULL;
    const json_t *msg = tms2_dict_get(result, "message", false, NULL, &raise);
    if (raise) return raise;
    json_t *out = json_object();
    json_set(out, "ok", json_bool(true));
    json_set(out, "message", msg ? json_copy(msg) : json_null());
    json_set(out, "payload", result ? json_copy(result) : json_null());
    return out;
}

/* _billing_route's generated handler: invalid(params) -> _billing_invalid;
 * idempotent mints/echoes idempotency_key; call(nb, params, key) | extra
 * rides _billing_call. name is decoration-time only (no event). */
/* PoP: tms2_billing_route @ tui_gateway/methods_session.py:_billing_route */
static json_t *tms2_billing_route(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    const json_t *invalid = json_obj_get(args, "invalid");
    bool idempotent = json_get_bool(args, "idempotent", false);
    if (invalid && invalid->type != JSON_NULL) {
        json_t *ia = tms2_args0();
        tms2_aset(ia, "params", params);
        json_t *iv = tms2_call(ctx, "invalid_fn", ia);
        if (tms2_is_raise(iv)) return iv;
        bool bad = tms2_truthy(iv);
        json_free(iv);
        if (bad) {
            json_t *ba = tms2_args0();
            tms2_aset(ba, "rid", rid);
            tms2_asets(ba, "message", json_get_str(args, "message", ""));
            tms2_asets(ba, "error", json_get_str(args, "error", "invalid_request"));
            return tms2_billing_invalid(ctx, ba);
        }
    }
    json_t *key = NULL;
    json_t *extra = NULL;
    if (idempotent) {
        json_t *raise = NULL;
        const json_t *pk = tms2_dict_get(params, "idempotency_key", false, NULL, &raise);
        if (raise) return raise;
        if (tms2_truthy(pk)) {
            key = json_copy(pk);
        } else {
            key = tms2_call(ctx, "new_idempotency_key", tms2_args0());
            if (tms2_is_raise(key)) return key;
        }
        extra = json_object();
        json_set(extra, "idempotency_key", json_copy(key));
    }
    json_t *r = tms2_billing_call_core(ctx, rid, 1, params, key, extra);
    json_free(key);
    json_free(extra);
    return r;
}

/* ======================= unarchive / title lookup ======================= */

/* _row_of on a borrowed record node (composed sibling core). */
static json_t *tms2_row_of_v(tms2_ctx_t *ctx, const json_t *record) {
    if (!record || record->type == JSON_NULL) return json_null();
    return tms2_as_session_row(ctx, record);
}

/* unarchive_recoverable on a read-only listing handle escalates to a
 * short-lived writer handle; the writer close is suppress(Exception). */
/* PoP: tms2_unarchive_recoverable @ tui_gateway/methods_session.py:_unarchive_recoverable */
static json_t *tms2_unarchive_recoverable(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *p = json_obj_get(args, "persistence");
    const json_t *sid = json_obj_get(args, "session_id");
    json_t *raise = NULL;
    const json_t *ro = tms2_attr(p, "read_only", false, NULL, &raise);
    if (raise) return raise;
    if (!tms2_truthy(ro)) {
        json_t *bad = tms2_persist_sub(p, "sessions");
        if (bad) return bad;
        json_t *a = tms2_args0();
        tms2_aset(a, "session_id", sid);
        return tms2_call(ctx, "unarchive_recoverable", a);
    }
    /* try: writer = open_persistence(persistence.scope, eager=True) */
    json_t *writer = NULL;
    json_t *exc = NULL;
    const json_t *scope = tms2_attr(p, "scope", false, NULL, &raise);
    if (raise) {
        exc = raise;
    } else {
        json_t *oa = tms2_args0();
        tms2_aset(oa, "scope", scope);
        tms2_asetb(oa, "eager", true);
        writer = tms2_call(ctx, "open_persistence", oa);
        if (tms2_is_raise(writer)) { exc = writer; writer = NULL; }
    }
    if (exc) {
        if (tms2_is_base_exc(tms2_raise_kind(exc))) return exc;
        json_free(exc);
        /* logger.warning(... persistence.scope.profile_dir ...) -- re-read */
        const json_t *scope2 = tms2_attr(p, "scope", false, NULL, &raise);
        if (raise) return raise;
        const json_t *pd = tms2_attr(scope2, "profile_dir", false, NULL, &raise);
        if (raise) return raise;
        char *pds = tms2_pystr(pd);
        size_t mn = strlen(pds) + 64;
        char *msg = malloc(mn);
        if (msg) snprintf(msg, mn,
                          "Bot Chat unarchive skipped: writer unavailable for %s", pds);
        free(pds);
        json_t *la = tms2_args0();
        tms2_asets(la, "msg", msg ? msg : "");
        json_t *lg = tms2_call(ctx, "logger_warning", la);
        free(msg);
        if (tms2_is_raise(lg)) return lg;
        json_free(lg);
        return json_bool(false);
    }
    /* try: return writer.sessions.unarchive_recoverable(sid)
     * finally: with suppress(Exception): writer.close() */
    json_t *ret = NULL;
    json_t *wexc = NULL;
    if (!writer || writer->type != JSON_OBJECT) {
        char msg[192];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'sessions'",
                 tms2_type_name(writer));
        wexc = tms2_err_spec("AttributeError", msg);
    } else {
        json_t *a = tms2_args0();
        tms2_aset(a, "session_id", sid);
        ret = tms2_call(ctx, "writer_unarchive", a);
        if (tms2_is_raise(ret)) { wexc = ret; ret = NULL; }
    }
    json_t *cexc = NULL;
    if (!writer || writer->type != JSON_OBJECT) {
        char msg[192];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'close'",
                 tms2_type_name(writer));
        cexc = tms2_err_spec("AttributeError", msg);
    } else {
        cexc = tms2_call(ctx, "writer_close", tms2_args0());
        if (!tms2_is_raise(cexc)) { json_free(cexc); cexc = NULL; }
    }
    if (cexc) {
        if (tms2_is_base_exc(tms2_raise_kind(cexc))) {
            json_free(wexc); json_free(ret); json_free(writer);
            return cexc;
        }
        json_free(cexc);   /* contextlib.suppress(Exception) */
    }
    json_free(writer);
    if (wexc) return wexc;
    return ret;
}

/* EXACT-title lookup: archived Bot Chat resurrects recoverable archives;
 * archived / deny-listed rows hide; lineages resolve to the live tip. */
/* PoP: tms2_session_list_by_title @ tui_gateway/methods_session.py:_session_list_by_title */
static json_t *tms2_session_list_by_title(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *p = json_obj_get(args, "persistence");
    const char *title = json_get_str(args, "title_lookup", "");
    json_t *rec = tms2_get_by_title(ctx, p, title);
    if (tms2_is_raise(rec)) return rec;
    json_t *row = tms2_row_of_v(ctx, rec);
    json_free(rec);
    if (tms2_is_raise(row)) return row;
    json_t *raise = NULL;
    bool archived0 = false;
    if (tms2_truthy(row)) {
        const json_t *arch0 = tms2_dict_get(row, "archived", false, NULL, &raise);
        if (raise) { json_free(row); return raise; }
        archived0 = tms2_truthy(arch0);
    }
    if (archived0 && !strcmp(title, "Bot Chat")) {
        const json_t *id = tms2_sub(row, "id", &raise);
        if (raise) { json_free(row); return raise; }
        json_t *ua = tms2_args0();
        tms2_aset(ua, "persistence", p);
        tms2_aset(ua, "session_id", id);
        json_t *un = tms2_unarchive_recoverable(ctx, ua);
        if (tms2_is_raise(un)) { json_free(row); return un; }
        bool ok = tms2_truthy(un);
        json_free(un);
        if (ok) {
            /* Re-fetch by ID -- title is not UNIQUE. */
            json_t *rec2 = tms2_sessions_get(ctx, p, id);
            if (tms2_is_raise(rec2)) { json_free(row); return rec2; }
            json_t *row2 = tms2_row_of_v(ctx, rec2);
            json_free(rec2);
            if (tms2_is_raise(row2)) { json_free(row); return row2; }
            json_free(row);
            row = row2;
        }
    }
    bool hide = !tms2_truthy(row);
    if (!hide) {
        const json_t *arch1 = tms2_dict_get(row, "archived", false, NULL, &raise);
        if (raise) { json_free(row); return raise; }
        if (tms2_truthy(arch1)) hide = true;
    }
    if (!hide) {
        json_t *denied = tms2_denied_source(ctx, row);
        if (tms2_is_raise(denied)) { json_free(row); return denied; }
        hide = tms2_truthy(denied);
        json_free(denied);
    }
    if (hide) {
        json_free(row);
        json_t *payload = json_object();
        json_set(payload, "sessions", json_array());
        return tms2_ok(ctx, rid, payload);
    }
    raise = NULL;
    const json_t *id = tms2_sub(row, "id", &raise);
    if (raise) { json_free(row); return raise; }
    json_t *tip = json_copy(id);
    /* with suppress(Exception): tip = lineage.tip(row["id"]) or row["id"] */
    json_t *lt = tms2_lineage_tip(ctx, p, id);
    if (tms2_is_raise(lt)) {
        if (tms2_is_base_exc(tms2_raise_kind(lt))) {
            json_free(tip); json_free(row);
            return lt;
        }
        json_free(lt);
    } else if (tms2_truthy(lt)) {
        json_free(tip);
        tip = lt;
    } else {
        json_free(lt);
    }
    json_t *tip_row;
    if (!tms2_json_eq(tip, id)) {
        json_t *rec3 = tms2_sessions_get(ctx, p, tip);
        if (tms2_is_raise(rec3)) { json_free(tip); json_free(row); return rec3; }
        json_t *row3 = tms2_row_of_v(ctx, rec3);
        json_free(rec3);
        if (tms2_is_raise(row3)) { json_free(tip); json_free(row); return row3; }
        if (tms2_truthy(row3)) {
            tip_row = row3;
        } else {
            json_free(row3);
            tip_row = json_copy(row);
        }
    } else {
        tip_row = json_copy(row);
    }
    json_t *sa = tms2_args0();
    tms2_aset(sa, "row", row);
    tms2_aset(sa, "tip_row", tip_row);
    tms2_aset(sa, "resolved_id", tip);
    tms2_aset(sa, "persistence", p);
    json_t *summary = tms2_session_row_summary(ctx, sa);
    json_free(tip_row);
    json_free(tip);
    json_free(row);
    if (tms2_is_raise(summary)) return summary;
    json_t *sessions = json_array();
    json_append(sessions, summary);
    json_t *payload = json_object();
    json_set(payload, "sessions", sessions);
    return tms2_ok(ctx, rid, payload);
}

/* ======================= live lookup / status / title ======================= */

/* Runtime sid of a live, not-yet-persisted session matched by stored key or
 * pending title (profile_home gate first). */
/* PoP: tms2_find_live_unpersisted @ tui_gateway/methods_session.py:_find_live_unpersisted */
static json_t *tms2_find_live_unpersisted(tms2_ctx_t *ctx, const json_t *args) {
    const char *needle = json_get_str(args, "needle", "");
    const json_t *home = json_obj_get(args, "home");
    char *want_home = (home && home->type != JSON_NULL) ? tms2_pystr(home) : NULL;
    json_t *items = tms2_call(ctx, "sessions_items", tms2_args0());
    if (tms2_is_raise(items)) { free(want_home); return items; }
    json_t *bad = tms2_iter_check(items);
    if (bad) { json_free(items); free(want_home); return bad; }
    json_t *out = json_string("");
    for (size_t i = 0; i < items->c.count; i++) {
        json_t *u[2];
        json_t *ubad = tms2_unpack(items->c.items[i], 2, u);
        if (ubad) {
            json_free(out); json_free(items); free(want_home);
            return ubad;
        }
        bool match = false;
        if (u[1] && u[1]->type == JSON_OBJECT) {
            const json_t *ph = json_obj_get(u[1], "profile_home");
            bool ph_match;
            if (!tms2_truthy(ph)) ph_match = (want_home == NULL);
            else ph_match = want_home && tms2_str(ph)
                            && !strcmp(tms2_str(ph), want_home);
            if (ph_match) {
                const json_t *sk = json_obj_get(u[1], "session_key");
                char *sks = tms2_truthy(sk) ? tms2_pystr(sk) : tms2_strdup("");
                match = sks && !strcmp(sks, needle);
                free(sks);
                if (!match) {
                    const json_t *pt = json_obj_get(u[1], "pending_title");
                    const char *pts = tms2_truthy(pt) ? tms2_str(pt) : "";
                    match = pts && !strcmp(pts, needle);
                }
            }
        }
        if (match) {
            json_free(out);
            out = json_copy(u[0]);
            json_free(u[0]); json_free(u[1]);
            break;
        }
        json_free(u[0]); json_free(u[1]);
    }
    json_free(items);
    free(want_home);
    return out;
}

/* _try_get_session core: suppress(Exception) over the persistence read. */
static json_t *tms2_try_get_session_v(tms2_ctx_t *ctx, const json_t *db,
                                     const json_t *key) {
    json_t *pa = tms2_args0();
    tms2_aset(pa, "db", db);
    json_t *raise = NULL;
    json_t *p = tms2_cm_enter(ctx, "persistence_for", pa, &raise);
    if (raise) {
        if (tms2_is_base_exc(tms2_raise_kind(raise))) return raise;
        json_free(raise);
        return json_object();
    }
    json_t *rec = tms2_sessions_get(ctx, p, key);
    json_free(p);
    if (tms2_is_raise(rec)) {
        if (tms2_is_base_exc(tms2_raise_kind(rec))) return rec;
        json_free(rec);
        return json_object();
    }
    json_t *row = tms2_row_of_v(ctx, rec);
    json_free(rec);
    if (tms2_is_raise(row)) {
        if (tms2_is_base_exc(tms2_raise_kind(row))) return row;
        json_free(row);
        return json_object();
    }
    if (!tms2_truthy(row)) { json_free(row); return json_object(); }
    return row;
}

/* Stored row for key: the live session's bound profile db first, else
 * params.profile / launch. */
/* PoP: tms2_status_row @ tui_gateway/methods_session.py:_status_row */
static json_t *tms2_status_row(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *session = json_obj_get(args, "session");
    const json_t *params = json_obj_get(args, "params");
    const json_t *key = json_obj_get(args, "key");
    if (!tms2_truthy(key)) return json_object();
    json_t *da = tms2_args0();
    tms2_aset(da, "session", session);
    json_t *raise = NULL;
    json_t *db = tms2_cm_enter(ctx, "session_db", da, &raise);
    if (raise) return raise;
    json_t *ret;
    if (db) {
        ret = tms2_try_get_session_v(ctx, db, key);
    } else {
        json_t *pa = tms2_args0();
        tms2_aset(pa, "params", params);
        json_t *db2 = tms2_cm_enter(ctx, "profile_db", pa, &raise);
        if (raise) { json_free(db); return raise; }
        if (db2) ret = tms2_try_get_session_v(ctx, db2, key);
        else ret = json_object();
        json_free(db2);
    }
    json_free(db);
    return ret;
}

/* with suppress(Exception), _persistence_for(db) as persistence:
 *     return _row_of(persistence.sessions.get(key)) or {}
 * return {} */
/* PoP: tms2_try_get_session @ tui_gateway/methods_session.py:_try_get_session */
static json_t *tms2_try_get_session(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *db = json_obj_get(args, "db");
    const json_t *key = json_obj_get(args, "key");
    return tms2_try_get_session_v(ctx, db, key);
}

/* session.title without title: read it, applying a queued pending_title if
 * possible. The session dict is the caller-state mirror (mutated in place). */
/* PoP: tms2_title_read @ tui_gateway/methods_session.py:_title_read */
static json_t *tms2_title_read(tms2_ctx_t *ctx, const json_t *args) {
    json_t *session = tms2_state_key(ctx, "session");
    const json_t *p = json_obj_get(args, "persistence");
    const json_t *key = json_obj_get(args, "key");
    json_t *raise = NULL;
    const json_t *pt = tms2_dict_get(session, "pending_title", false, NULL, &raise);
    if (raise) return raise;
    json_t *fallback = tms2_truthy(pt) ? json_copy(pt) : json_string("");
    json_t *resolved = NULL;
    json_t *exc = NULL;
    /* resolved_title = persistence.sessions.get_title(key) or "" */
    json_t *bad = tms2_persist_sub(p, "sessions");
    if (bad) {
        exc = bad;
    } else {
        json_t *ga = tms2_args0();
        tms2_aset(ga, "key", key);
        json_t *gt = tms2_call(ctx, "get_title", ga);
        if (tms2_is_raise(gt)) { exc = gt; }
        else {
            resolved = tms2_truthy(gt) ? gt : json_string("");
            if (resolved != gt) json_free(gt);
        }
    }
    if (!exc) {
        if (!tms2_truthy(fallback)) {
            if (tms2_truthy(resolved))
                json_set(session, "pending_title", json_null());
        } else {
            bool set_ok = false;
            json_t *sa = tms2_args0();
            tms2_aset(sa, "key", key);
            tms2_aset(sa, "title", fallback);
            json_t *st = tms2_call(ctx, "set_title", sa);
            if (tms2_is_raise(st)) { exc = st; }
            else { set_ok = tms2_truthy(st); json_free(st); }
            if (!exc && !set_ok) {
                json_t *rec = tms2_sessions_get(ctx, p, key);
                if (tms2_is_raise(rec)) { exc = rec; }
                else {
                    json_t *row = tms2_row_of_v(ctx, rec);
                    json_free(rec);
                    if (tms2_is_raise(row)) { exc = row; }
                    else {
                        json_t *empty = json_object();
                        const json_t *rd = tms2_truthy(row) ? row : empty;
                        const json_t *title = tms2_dict_get(rd, "title", false, NULL, &raise);
                        if (raise) { exc = raise; }
                        else if (tms2_truthy(title) && title->type != JSON_STRING) {
                            char msg[192];
                            snprintf(msg, sizeof msg,
                                     "'%s' object has no attribute 'strip'",
                                     tms2_type_name(title));
                            exc = tms2_err_spec("AttributeError", msg);
                        } else {
                            const char *ts = tms2_truthy(title) ? tms2_str(title) : "";
                            char *tstrip = tms2_strip(ts);
                            const char *fs = tms2_str(fallback);
                            set_ok = fs && tstrip && !strcmp(tstrip, fs);
                            free(tstrip);
                        }
                        json_free(empty);
                        json_free(row);
                    }
                }
            }
            if (!exc) {
                if (set_ok) {
                    json_set(session, "pending_title", json_null());
                    json_free(resolved);
                    resolved = json_copy(fallback);
                } else if (!tms2_truthy(resolved)) {
                    json_free(resolved);
                    resolved = json_copy(fallback);
                }
            }
        }
    }
    if (exc) {
        if (tms2_is_base_exc(tms2_raise_kind(exc))) {
            json_free(fallback); json_free(resolved);
            return exc;
        }
        json_free(exc);
        json_free(resolved);
        resolved = json_copy(fallback);
    }
    json_free(fallback);
    return resolved;
}

/* ======================= auto-reload / spawn-tree ======================= */

/* PATCH /api/billing/auto-top-up; returns {"ok": True}. */
/* PoP: tms2_auto_reload @ tui_gateway/methods_session.py:_auto_reload */
static json_t *tms2_auto_reload(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *p = json_obj_get(args, "p");
    json_t *raise = NULL;
    const json_t *en = tms2_dict_get(p, "enabled", false, NULL, &raise);
    if (raise) return raise;
    const json_t *th = tms2_dict_get(p, "threshold", false, NULL, &raise);
    if (raise) return raise;
    const json_t *ta = tms2_dict_get(p, "top_up_amount", false, NULL, &raise);
    if (raise) return raise;
    json_t *a = tms2_args0();
    tms2_asetb(a, "enabled", tms2_truthy(en));
    tms2_aset(a, "threshold", th);
    tms2_aset(a, "top_up_amount", ta);
    json_t *r = tms2_call(ctx, "patch_auto_top_up", a);
    if (tms2_is_raise(r)) return r;
    json_free(r);
    json_t *out = json_object();
    json_set(out, "ok", json_bool(true));
    return out;
}

/* Index-shaped entry for a pre-index snapshot file (None when unreadable). */
/* PoP: tms2_legacy_spawn_tree_entry @ tui_gateway/methods_session.py:_legacy_spawn_tree_entry */
static json_t *tms2_legacy_spawn_tree_entry(tms2_ctx_t *ctx, const json_t *args) {
    const char *path = json_get_str(args, "path", "");
    const char *dirname = json_get_str(args, "session_dir_name", "");
    /* try: stat = p.stat() except OSError: return None */
    json_t *sa = tms2_args0();
    tms2_asets(sa, "path", path);
    json_t *stat = tms2_call(ctx, "stat", sa);
    if (tms2_is_raise(stat)) {
        if (tms2_is_oserror(tms2_raise_kind(stat))) { json_free(stat); return json_null(); }
        return stat;
    }
    /* raw = {}; with suppress(Exception): raw = json.loads(p.read_text(...)) */
    json_t *raw = json_object();
    json_t *ra = tms2_args0();
    tms2_asets(ra, "path", path);
    tms2_asets(ra, "encoding", "utf-8");
    json_t *txt = tms2_call(ctx, "read_text", ra);
    if (tms2_is_raise(txt)) {
        if (tms2_is_base_exc(tms2_raise_kind(txt))) {
            json_free(raw); json_free(stat);
            return txt;
        }
        json_free(txt);
    } else {
        const char *ts = tms2_str(txt);
        json_t *parsed = NULL;
        if (ts && tms2_json_loads(ts, &parsed) == 0) {
            json_free(raw);
            raw = parsed;
        }
        json_free(txt);
    }
    if (raw->type != JSON_OBJECT) { json_free(raw); raw = json_object(); }
    const json_t *subs = json_obj_get(raw, "subagents");
    json_t *out = json_object();
    json_set_hdr(out, "path", path);
    const json_t *sid = json_obj_get(raw, "session_id");
    json_set(out, "session_id", tms2_truthy(sid) ? json_copy(sid) : json_string(dirname));
    const json_t *fin = json_obj_get(raw, "finished_at");
    if (tms2_truthy(fin)) {
        json_set(out, "finished_at", json_copy(fin));
    } else {
        json_t *raise = NULL;
        const json_t *mt = tms2_attr(stat, "st_mtime", false, NULL, &raise);
        if (raise) { json_free(out); json_free(raw); json_free(stat); return raise; }
        json_set(out, "finished_at", json_copy(mt));
    }
    const json_t *st = json_obj_get(raw, "started_at");
    json_set(out, "started_at", st ? json_copy(st) : json_null());
    const json_t *label = json_obj_get(raw, "label");
    json_set(out, "label", tms2_truthy(label) ? json_copy(label) : json_string(""));
    double count = (subs && subs->type == JSON_ARRAY) ? (double)subs->c.count : 0.0;
    json_set(out, "count", json_number(count));
    json_free(raw);
    json_free(stat);
    return out;
}