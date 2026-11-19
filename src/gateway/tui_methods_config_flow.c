/*
 * tui_methods_config_flow.c -- FLOW + leaf port of tui_gateway/methods_config.py
 * (hermes-agent): the config / projects / setup-readiness / diagnostics JSON-RPC
 * helper surface -- display-word normalization, the config.get getter table
 * (named getters + lambdas), repo-discovery project handlers, the single-flighted
 * readiness shell, client-text redaction and the share_nous upload envelope.
 *
 * The module's functions are rebound onto server.py's globals at install time
 * (method_ctx.bind_module), so _err / _ok / _load_cfg / _display_cfg / _sessions /
 * _hermes_home / _profile_db / logger / the agent_factory + methods_projects +
 * session_workdir + model_switch helpers are bare free variables. ALL of them are
 * seams here (external to methods_config.py). _ok / _load_cfg / _display_cfg are
 * CLAIMED by tui_server_flow.c -- seamed, never re-ported.
 *
 * Ports the DECISION SEQUENCES -- branch order, error-code/message composition,
 * seam call order, dict-literal evaluation order and exception paths. Every live
 * external call enters through ONE universal seam: tmc_call(ctx, name, args).
 * The harness wires it to a recording stub fed the case's scripted results
 * (in.io); the oracle installs recording stubs emitting the identical event
 * shape {"name","args","ret"} from the SAME scripts.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and tmc_-prefixed.
 *
 * Decorated handlers (def _ under @method / @_profile_scoped / @_projects_handler)
 * are ported BARE (registry decorators stripped): tmc_config_get, tmc_setup_status,
 * tmc_setup_runtime_check, tmc_share_nous, tmc_projects_discover_repos,
 * tmc_projects_record_repos, tmc_projects_tree, tmc_projects_project_sessions.
 * The _projects_handler wrapper itself is ported separately (tmc_projects_handler)
 * and driven with a callback fn seam; its registration seams fire in Python
 * evaluation order: method(name) BEFORE _profile_scoped(handler).
 * The _CONFIG_GETTERS lambdas are internal helpers (tmc_get_*, no PoP -- lambdas
 * have no Python def name); they compose through tmc_config_get / the
 * tmc_config_getter dispatch and are exercised by their own cases.
 * Ported siblings compose LIVE where they are leaf-flows (_display_raw inside
 * _display_word / skin / indicator / density / theme / details_mode /
 * thinking_mode; _display_word inside thinking_mode; _redact_client_text and
 * _safe_client_label inside share_nous; _reconcile_repo_discovery inside the
 * project handlers; _stamped_project_tree inside tree / project_sessions;
 * _readiness_share inside _readiness_check; _readiness_check inside setup.status
 * / setup.runtime_check; the cfg getters inside config.get).
 *
 * Seam name -> live call (args are informational projections):
 *   err                          _err(rid, code, msg)  [server global]
 *   ok                           _ok(rid, result)  [claimed sibling]
 *   load_cfg                     _load_cfg()  [claimed sibling]
 *   display_cfg                  _display_cfg()  [claimed sibling]
 *   resolve_model                _resolve_model()  [agent_factory]
 *   normalize_provider           hermes_cli.models.normalize_provider(parts0)
 *   list_available_providers     hermes_cli.models.list_available_providers()
 *   completion_cwd               _completion_cwd({cwd, cwd_explicit, profile})
 *   git_branch                   git_probe.branch(cwd)
 *   active_personality_name      hermes_cli.personality.active_personality_name(cfg)
 *   sessions_get                 _sessions.get(session_id)  [server dict]
 *   session_runtime_view         agent.voice_turn_route.session_runtime_view(agent)
 *   parse_reasoning_effort       hermes_constants.parse_reasoning_effort(raw)
 *   load_service_tier            _load_service_tier()  [agent_factory]
 *   compute_mcp_rev              _compute_mcp_rev()  [server]
 *   config_version_or_none       hermes_cli.config_backend.config_version_or_none(path)
 *   version_token                hermes_cli.config_backend.version_token(version)
 *   display_hermes_home          hermes_constants.display_hermes_home()
 *   load_busy_input_mode         _load_busy_input_mode()  [server]
 *   load_approval_mode           _load_approval_mode()  [agent_factory]
 *   coerce_statusbar             _coerce_statusbar(raw)  [agent_factory]
 *   load_tool_progress_mode      _load_tool_progress_mode()  [agent_factory]
 *   display_mouse_tracking       _display_mouse_tracking(display)  [agent_factory]
 *   repo_discovery_policy        _repo_discovery_policy(raw=None)  [methods_projects]
 *   repo_discovery_policy_key    _repo_discovery_policy_key(policy)
 *   repo_discovery_policy_is_default  _repo_discovery_policy_is_default(policy)
 *   reconcile_discovered_repos_policy pdb.reconcile_discovered_repos_policy(
 *                                conn, policy_key, preserve_unversioned=...)
 *   record_discovered_repos      pdb.record_discovered_repos(conn, pairs,
 *                                replace=True, policy_key=...)
 *   clear_discovered_repos       pdb.clear_discovered_repos(conn, policy_key=...)
 *   scan_discovered_repos_remote _scan_discovered_repos_remote(conn, policy)
 *   discover_repos_payload       _discover_repos_payload(db, conn=..., include_cached=...)
 *   profile_db                   _profile_db(params) -> YIELDED db (CM enter/exit
 *                                silent; null == yielded None, a valid db)
 *   connect_closing              pdb.connect_closing() -> YIELDED conn (CM silent)
 *   build_project_tree           _build_project_tree(db, **kwargs) -> [tree, active_id]
 *   response_profile_name        _response_profile_name(profile)
 *   stamp_profile                tui_gateway.project_tree.stamp_profile(projects, name)
 *   profile_exists               hermes_cli.profiles.profile_exists(profile)
 *   profile_home                 _profile_home(profile) -> Path posix string or null
 *   session_profile_runtime_scope _session_profile_runtime_scope({profile_home})
 *                                -> CM marker (dict); null/non-dict raises
 *                                TypeError at the live `with` site
 *   pool_submit                  _readiness_pool.submit(run_probe) -> future marker
 *                                {"id": ...}. SINGLE-THREADED MODEL: the submit
 *                                double runs run_probe synchronously right after
 *                                the pool_submit event (probe outcome is stored
 *                                on the future; future.result re-scripts it).
 *   add_done_callback            future.add_done_callback(cb)  [cb NOT invoked --
 *                                _readiness_cleared has its own cases]
 *   future_result                future.result(timeout=t)
 *   future_cancelled             future.cancelled()
 *   future_exception             future.exception() -> exception VALUE: script
 *                                {"__raise__": kind, "message": msg} materializes
 *                                a real exception instance (canon projects it
 *                                back to the same envelope); null == None
 *   run_probe                    the run_probe() callback (direct _readiness_share)
 *   probe                        the probe(profile, scoped) callback (direct
 *                                _readiness_check)
 *   logger_warning               logger.warning(fmt, *args) -> {"msg": fmt % args}
 *   logger_debug                 logger.debug(fmt, *args) -> {"msg": fmt % args}
 *   wait_for_record              hermes_cli.free_tier_bootstrap.wait_for_record()
 *                                -> record double (dict -> SimpleNamespace with
 *                                a failure_fields() method) or null
 *   has_any_provider_configured  hermes_cli.main._has_any_provider_configured(
 *                                strict_profile_scope=...)
 *   free_tier_route              hermes_cli.anon_auth.free_tier_route()
 *   failure_fields               record.failure_fields() -> dict (**-merged)
 *   resolve_startup_runtime      _resolve_startup_runtime() -> [model, provider]
 *   resolve_runtime_provider     hermes_cli.runtime_provider.resolve_runtime_provider(
 *                                requested=..., target_model=...)
 *   resolve_agent_model_runtime  _resolve_agent_model_runtime(None, None)
 *                                -> [model, runtime]
 *   runtime_has_credentials      hermes_cli.runtime_readiness.runtime_has_credentials(runtime)
 *   missing_credentials_suffix   hermes_cli.runtime_readiness.missing_credentials_suffix(provider)
 *   route_is_welcome_host        hermes_cli.anon_auth.route_is_welcome_host(base_url)
 *   readiness_failure            hermes_cli.runtime_readiness.readiness_failure(exc)
 *                                args {type, message}
 *   redact_debug_support_text    hermes_cli.debug_redaction.redact_debug_support_text(
 *                                value, max_chars=...) -- value str()ed (exception
 *                                instances project to str(exc) == message)
 *   collect_share_bundle         hermes_cli.debug.collect_share_bundle(log_lines=..., redact=True)
 *   build_nous_bundle            hermes_cli.debug.build_nous_bundle(bundle, redact=True)
 *   share_to_nous                hermes_cli.diagnostics_upload.share_to_nous(bundle)
 *   projects_fn                  the fn(rid, params) callback inside _projects_handler
 *   method                       registry method(name) -> identity decorator
 *   profile_scoped               registry _profile_scoped(handler) -> handler
 *
 * Representation contract (shared with tests/oracle_tui_methods_config_flow.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)}; BaseException-only
 *   kinds (SystemExit, KeyboardInterrupt, GeneratorExit, CancelledError) PASS
 *   THROUGH every except-Exception site.
 * - `is False` / `is True` identity gates (reasoning enabled, daybreak) match
 *   ONLY the JSON bool -- 0/1/null are NOT False/True.
 * - Record/agent doubles: scripted dicts become SimpleNamespaces (attribute
 *   access; missing attr raises "'types.SimpleNamespace' object has no attribute
 *   '<key>'"); session dicts stay raw dicts but their "agent" field becomes an
 *   attribute double. Non-dict attribute targets raise the live type-name
 *   AttributeError at the access site.
 * - Readiness inflight state: ctx.state.inflight is an ORDERED list of
 *   [key, future_id] pairs; key is [probe_key, profile]. Models the live
 *   dict[tuple, Future] (identity == id equality; insertion order preserved).
 *   _readiness_lock is silent (single-threaded).
 * - _hermes_home is injected via args.hermes_home (absolute POSIX path); the
 *   oracle drives it as PurePosixPath so str() stays POSIX and `/` joins append
 *   a component (path join is string concat on the fixture domain).
 * - Tuples project to lists; sets to sorted lists; integral floats to ints;
 *   Paths to POSIX strings. Fixture numbers are ints except wait_seconds
 *   (4.0/12.0/0.5) and int()-coercion inputs; unpack seams script 2-lists or
 *   null/int (never str). str()/strip()/lower() fold the ASCII corpus only
 *   (strip ws set includes \x1c-\x1f).
 * - Seam args drop None-valued keys on both sides (absent-vs-null parity).
 * - Fail-closed: a missing/exhausted script is a RECORDED RuntimeError raise
 *   {"no script for X" / "script exhausted for X"} on both sides.
 *
 * Ownership: tmc_call ADOPTS args into the trace event; its return is a fresh
 * owned node. json_set ADOPTS its value and frees the replaced node. Seam
 * results are caller-freed.
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
typedef struct tmc_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
    bool last_raise;    /* the last tmc_call produced a scripted/fail-closed
                         * raise (vs a returned exception VALUE envelope) */
} tmc_ctx_t;

/* -- basic helpers -- */
static bool tmc_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static const char *tmc_raise_kind(const json_t *v) {
    return tmc_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}

/* BaseException-only kinds pass through every except-Exception site. */
static bool tmc_is_base_exc(const char *kind) {
    return kind && (!strcmp(kind, "SystemExit") || !strcmp(kind, "KeyboardInterrupt")
                    || !strcmp(kind, "GeneratorExit") || !strcmp(kind, "CancelledError"));
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool tmc_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static const char *tmc_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static json_t *tmc_err_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* Python type name for a JSON value (fixture scalar/container domain). */
static const char *tmc_type_name(const json_t *v) {
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

static char *tmc_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

/* Python str() for scalars (containers are outside the str()-projection
 * fixture domain). */
static char *tmc_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return tmc_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return tmc_strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return tmc_strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && d > -9.2e18 && d < 9.2e18)
                snprintf(buf, sizeof buf, "%lld", (long long)d);
            else
                snprintf(buf, sizeof buf, "%.17g", d);
            return tmc_strdup(buf);
        }
        default: return json_serialize(v);
    }
}

/* Python repr() for strings (ASCII corpus; quote choice minimises escapes). */
static char *tmc_repr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return tmc_strdup("None");
    if (v->type != JSON_STRING) return tmc_pystr(v);
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
static bool tmc_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f'
        || c == '\v' || (c >= 0x1c && c <= 0x1f);
}

static char *tmc_strip(const char *s) {
    if (!s) s = "";
    while (*s && tmc_is_ws((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && tmc_is_ws((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - s);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

static char *tmc_lower(const char *s) {
    char *o = tmc_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}

/* Deep scalar equality (Python == on the fixture domain; bool==int cross). */
static bool tmc_json_eq(const json_t *a, const json_t *b) {
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
            if (!tmc_json_eq(a->c.items[i], b->c.items[i])) return false;
        return true;
    }
    if (a->type == JSON_OBJECT) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !tmc_json_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}

/* POSIX path join (fixture domain: absolute, no trailing slash, no "."
 * components; PurePosixPath semantics). */
static char *tmc_path_join(const char *dir, const char *leaf) {
    if (!dir) dir = "";
    if (!leaf) leaf = "";
    if (!dir[0] || !strcmp(dir, ".")) return tmc_strdup(leaf);
    size_t n = strlen(dir) + 1 + strlen(leaf);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    snprintf(o, n + 1, "%s/%s", dir, leaf);
    return o;
}

/* str((a, b)) for the readiness key tuple -- repr of each string element. */
static char *tmc_key_str(const json_t *key) {
    const json_t *a = json_get(key, 0);
    const json_t *b = json_get(key, 1);
    char *ra = tmc_repr(a);
    char *rb = tmc_repr(b);
    size_t n = strlen(ra) + strlen(rb) + 8;
    char *o = malloc(n);
    if (o) snprintf(o, n, "(%s, %s)", ra, rb);
    free(ra);
    free(rb);
    return o;
}

/* Python int() on the fixture domain (str/int/bool/float; null/list/dict raise
 * the live TypeError). Returns a fresh number node or a raise spec. */
static json_t *tmc_pyint(const json_t *v) {
    if (v && v->type == JSON_BOOL) return json_number(v->bool_val ? 1.0 : 0.0);
    if (v && v->type == JSON_NUMBER) {
        double d = v->num_val;
        return json_number((double)(long long)d); /* trunc toward zero */
    }
    if (v && v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        /* int()'s whitespace set is \t\n\v\f\r + space ONLY -- str.strip()'s
         * \x1c-\x1f are NOT accepted by the int() parser. */
        const char *b = s;
        while (*b == ' ' || (*b >= '\t' && *b <= '\r')) b++;
        const char *endp = s + strlen(s);
        while (endp > b && (endp[-1] == ' ' || (endp[-1] >= '\t' && endp[-1] <= '\r'))) endp--;
        size_t cn = (size_t)(endp - b);
        char *clean = malloc(cn + 1);
        if (!clean) return NULL;
        memcpy(clean, b, cn);
        clean[cn] = '\0';
        const char *p = clean;
        bool neg = false;
        if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
        long long acc = 0;
        size_t digits = 0;
        bool prev_digit = false;
        bool ok = true;
        for (; *p; p++) {
            unsigned char c = (unsigned char)*p;
            if (c >= '0' && c <= '9') {
                acc = acc * 10 + (c - '0');
                digits++;
                prev_digit = true;
            } else if (c == '_' && prev_digit && p[1] >= '0' && p[1] <= '9') {
                prev_digit = false; /* underscore must sit between digits */
            } else { ok = false; break; }
        }
        if (ok && digits > 0) {
            free(clean);
            return json_number((double)(neg ? -acc : acc));
        }
        free(clean);
        char *r = tmc_repr(v);
        size_t n = strlen(r) + 96;
        char *msg = malloc(n);
        if (msg) snprintf(msg, n, "invalid literal for int() with base 10: %s", r);
        free(r);
        json_t *e = tmc_err_spec("ValueError", msg ? msg : "");
        free(msg);
        return e;
    }
    char msg[160];
    snprintf(msg, sizeof msg,
             "int() argument must be a string, a bytes-like object or a real number, not '%s'",
             tmc_type_name(v));
    return tmc_err_spec("TypeError", msg);
}

/* a, b = v -- the live unpack messages (fixture domain: 2-lists, null/int/bool/
 * dict; str is iterable live but outside the unpack-script domain). */
static json_t *tmc_unpack2(const json_t *v, json_t **a, json_t **b) {
    *a = *b = NULL;
    if (v && v->type == JSON_ARRAY) {
        if (v->c.count < 2) {
            char msg[96];
            snprintf(msg, sizeof msg, "not enough values to unpack (expected 2, got %zu)",
                     v->c.count);
            return tmc_err_spec("ValueError", msg);
        }
        if (v->c.count > 2) {
            char msg[96];
            snprintf(msg, sizeof msg, "too many values to unpack (expected 2, got %zu)",
                     v->c.count);
            return tmc_err_spec("ValueError", msg);
        }
        *a = json_copy(v->c.items[0]);
        *b = json_copy(v->c.items[1]);
        return NULL;
    }
    char msg[96];
    snprintf(msg, sizeof msg, "cannot unpack non-iterable %s object", tmc_type_name(v));
    return tmc_err_spec("TypeError", msg);
}

/* d[key] -- borrowed value or the live KeyError/TypeError spec. */
static const json_t *tmc_subscript(const json_t *obj, const char *key, json_t **raise) {
    *raise = NULL;
    if (obj && obj->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(obj, key);
        if (v) return v;
        char msg[192];
        snprintf(msg, sizeof msg, "'%s'", key); /* str(KeyError) == repr(key) */
        *raise = tmc_err_spec("KeyError", msg);
        return NULL;
    }
    char msg[160];
    if (!obj || obj->type == JSON_NULL)
        snprintf(msg, sizeof msg, "'NoneType' object is not subscriptable");
    else if (obj->type == JSON_STRING)
        snprintf(msg, sizeof msg, "string indices must be integers, not 'str'");
    else if (obj->type == JSON_ARRAY)
        snprintf(msg, sizeof msg, "list indices must be integers or slices, not str");
    else
        snprintf(msg, sizeof msg, "'%s' object is not subscriptable", tmc_type_name(obj));
    *raise = tmc_err_spec("TypeError", msg);
    return NULL;
}

/* Attribute access on a SimpleNamespace double: borrowed value (or borrowed
 * default when has_default and absent); missing required attribute raises the
 * live AttributeError. Non-dict targets raise the type-name message. */
static const json_t *tmc_attr(const json_t *obj, const char *key,
                              bool has_default, const json_t *def, json_t **raise) {
    *raise = NULL;
    if (obj && obj->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(obj, key);
        if (v) return v;
        if (has_default) return def;
        char msg[192];
        snprintf(msg, sizeof msg,
                 "'types.SimpleNamespace' object has no attribute '%s'", key);
        *raise = tmc_err_spec("AttributeError", msg);
        return NULL;
    }
    if (has_default) return def;
    char msg[192];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'",
             tmc_type_name(obj), key);
    *raise = tmc_err_spec("AttributeError", msg);
    return NULL;
}

/* x.get(...) on a non-dict -- the live AttributeError naming 'get'. */
static json_t *tmc_get_error(const json_t *v) {
    char msg[160];
    snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", tmc_type_name(v));
    return tmc_err_spec("AttributeError", msg);
}

/* `with <non-CM>:` -- the py3.14 context-manager protocol TypeError. */
static json_t *tmc_cm_error(const json_t *v) {
    char msg[192];
    snprintf(msg, sizeof msg,
             "'%s' object does not support the context manager protocol "
             "(missed __exit__ method)", tmc_type_name(v));
    return tmc_err_spec("TypeError", msg);
}

/* {**v} on a non-mapping. */
static json_t *tmc_mapping_error(const json_t *v) {
    char msg[128];
    snprintf(msg, sizeof msg, "'%s' object is not a mapping", tmc_type_name(v));
    return tmc_err_spec("TypeError", msg);
}

/* dict.get(unhashable) -- py3.14 message. */
static json_t *tmc_unhashable_error(const json_t *v) {
    char msg[160];
    const char *tn = tmc_type_name(v);
    snprintf(msg, sizeof msg, "cannot use '%s' as a dict key (unhashable type: '%s')", tn, tn);
    return tmc_err_spec("TypeError", msg);
}

static json_t *tmc_iter_error(const json_t *v) {
    char msg[128];
    snprintf(msg, sizeof msg, "'%s' object is not iterable", tmc_type_name(v));
    return tmc_err_spec("TypeError", msg);
}

/* -- the one universal seam (fail-closed) -- */
static json_t *tmc_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return tmc_err_spec(type, msg);
}

static json_t *tmc_call(tmc_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    ctx->last_raise = false;
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
        ret = tmc_err_spec("RuntimeError", msg);
        ctx->last_raise = true;
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = tmc_err_spec("RuntimeError", msg);
            ctx->last_raise = true;
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = tmc_make_exc(raise);
                ctx->last_raise = true;
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
static json_t *tmc_args0(void) { return json_object(); }
static void tmc_aset(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static void tmc_asets(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void tmc_asetb(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void tmc_asetn(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }

/* _err(rid, code, msg) / _ok(rid, result) -- server-global envelope seams. */
static json_t *tmc_err(tmc_ctx_t *ctx, const json_t *rid, int code, const char *msg) {
    json_t *a = tmc_args0();
    tmc_aset(a, "rid", rid);
    tmc_asetn(a, "code", (double)code);
    tmc_asets(a, "message", msg);
    return tmc_call(ctx, "err", a);
}

static json_t *tmc_ok(tmc_ctx_t *ctx, const json_t *rid, json_t *result) {
    json_t *a = tmc_args0();
    tmc_aset(a, "rid", rid);
    json_set(a, "result", result ? result : json_null()); /* adopted */
    return tmc_call(ctx, "ok", a);
}

/* -- readiness inflight state: ordered [key, future_id] pairs -- */
static json_t *tmc_inflight_arr(tmc_ctx_t *ctx) {
    if (!json_has(ctx->state, "inflight")) json_set(ctx->state, "inflight", json_array());
    return json_obj_get(ctx->state, "inflight");
}

static long tmc_inflight_find(tmc_ctx_t *ctx, const json_t *key) {
    json_t *arr = tmc_inflight_arr(ctx);
    for (size_t i = 0; i < json_len(arr); i++) {
        const json_t *pair = json_get(arr, i);
        if (tmc_json_eq(json_get(pair, 0), key)) return (long)i;
    }
    return -1;
}

static void tmc_inflight_set(tmc_ctx_t *ctx, const json_t *key, const json_t *id) {
    json_t *arr = tmc_inflight_arr(ctx);
    long i = tmc_inflight_find(ctx, key);
    if (i >= 0) {
        json_t *pair = json_get(arr, (size_t)i);
        json_array_set(pair, 1, id ? json_copy(id) : json_null());
        return;
    }
    json_t *pair = json_array();
    json_append(pair, json_copy(key));
    json_append(pair, id ? json_copy(id) : json_null());
    json_append(arr, pair);
}

static void tmc_inflight_pop(tmc_ctx_t *ctx, const json_t *key) {
    json_t *arr = tmc_inflight_arr(ctx);
    long i = tmc_inflight_find(ctx, key);
    if (i < 0) return;
    json_t *pair = json_get(arr, (size_t)i);
    json_array_remove(arr, (size_t)i);
    json_free(pair);
}

/* future marker {"id": ...} -> id string node (NULL for a null marker). */
static json_t *tmc_future_id(const json_t *marker) {
    if (marker && marker->type == JSON_OBJECT)
        return json_string(json_get_str(marker, "id", ""));
    return NULL;
}

/* -- pure leaves -- */

/* _safe_client_label: alnum/._- () filter, strip, [:64], collapse ".." runs,
 * lstrip("."), strip. ASCII corpus (isalnum == [a-zA-Z0-9]). */
static char *tmc_safe_client_label_v(const char *label) {
    if (!label) label = "";
    size_t n = strlen(label);
    char *buf = malloc(n + 1);
    if (!buf) return NULL;
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)label[i];
        if (isalnum(c) || c == '.' || c == '_' || c == '-' || c == ' '
            || c == '(' || c == ')')
            buf[k++] = (char)c;
    }
    buf[k] = '\0';
    char *s = tmc_strip(buf);
    free(buf);
    if (!s) return NULL;
    if (strlen(s) > 64) s[64] = '\0';
    /* while ".." in safe: safe = safe.replace("..", ".") -- one non-overlapping
     * pass per iteration, exactly like str.replace. */
    for (;;) {
        if (!strstr(s, "..")) break;
        size_t sn = strlen(s);
        char *o = malloc(sn + 1);
        if (!o) { free(s); return NULL; }
        size_t j = 0;
        for (size_t i = 0; s[i]; ) {
            if (s[i] == '.' && s[i + 1] == '.') { o[j++] = '.'; i += 2; }
            else o[j++] = s[i++];
        }
        o[j] = '\0';
        free(s);
        s = o;
    }
    char *p = s;
    while (*p == '.') p++;
    char *r = tmc_strip(p);
    free(s);
    return r;
}

/* PoP: tmc_safe_client_label @ tui_gateway/methods_config.py:_safe_client_label */
static json_t *tmc_safe_client_label(tmc_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *label = json_obj_get(args, "label");
    const char *ls = tmc_str(label);
    if (!ls) return tmc_iter_error(label); /* for ch in label */
    char *r = tmc_safe_client_label_v(ls);
    json_t *out = json_string(r ? r : "");
    free(r);
    return out;
}

/* _redact_client_text: the scrub window is text[:cut+1] where cut is the first
 * newline in [2*cap, 3*cap), else text[:2*cap]; then the redaction seam. */
static json_t *tmc_redact_client_text_call(tmc_ctx_t *ctx, const char *text, double cap) {
    size_t len = strlen(text);
    size_t lo = (size_t)(2.0 * cap), hi = (size_t)(3.0 * cap);
    long cut = -1;
    for (size_t i = lo; i < hi && i < len; i++) {
        if (text[i] == '\n') { cut = (long)i; break; }
    }
    size_t wlen = cut >= 0 ? (size_t)cut + 1 : (len < lo ? len : lo);
    char *window = malloc(wlen + 1);
    if (!window) return NULL;
    memcpy(window, text, wlen);
    window[wlen] = '\0';
    json_t *a = tmc_args0();
    tmc_asets(a, "text", window);
    tmc_asetn(a, "max_chars", cap);
    free(window);
    return tmc_call(ctx, "redact_debug_support_text", a);
}

/* PoP: tmc_redact_client_text @ tui_gateway/methods_config.py:_redact_client_text */
static json_t *tmc_redact_client_text(tmc_ctx_t *ctx, const json_t *args) {
    const json_t *text = json_obj_get(args, "text");
    const char *ts = tmc_str(text);
    if (!ts) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'find'", tmc_type_name(text));
        return tmc_err_spec("AttributeError", msg);
    }
    return tmc_redact_client_text_call(ctx, ts, json_get_num(args, "cap", 0));
}

/* -- display normalization -- */

/* _load_cfg().get("display") or {} */
static json_t *tmc_display_raw_v(tmc_ctx_t *ctx) {
    json_t *cfg = tmc_call(ctx, "load_cfg", tmc_args0());
    if (tmc_is_raise(cfg)) return cfg;
    if (!cfg || cfg->type != JSON_OBJECT) {
        json_t *e = tmc_get_error(cfg);
        json_free(cfg);
        return e;
    }
    const json_t *d = json_obj_get(cfg, "display");
    json_t *out = tmc_truthy(d) ? json_copy(d) : json_object();
    json_free(cfg);
    return out;
}

/* PoP: tmc_display_raw @ tui_gateway/methods_config.py:_display_raw */
static json_t *tmc_display_raw(tmc_ctx_t *ctx, const json_t *args) {
    (void)args;
    return tmc_display_raw_v(ctx);
}

static const char *const TMC_THINKING_MODES[] = {"collapsed", "truncated", "full"};
static const char *const TMC_DETAIL_MODES[] = {"hidden", "collapsed", "expanded"};
static const char *const TMC_INDICATOR_STYLES[] = {"ascii", "emoji", "kaomoji", "unicode"};
static const char *const TMC_THEMES[] = {"auto", "light", "dark"};

/* str(_display_raw().get(key, default) or "").strip().lower(); membership in
 * allowed else default. */
static json_t *tmc_display_word_v(tmc_ctx_t *ctx, const char *key, const char *def,
                                  const char *const *allowed, size_t n_allowed) {
    json_t *raw = tmc_display_raw_v(ctx);
    if (tmc_is_raise(raw)) return raw;
    const json_t *v = json_obj_get(raw, key);
    char *s;
    if (v) s = tmc_truthy(v) ? tmc_pystr(v) : tmc_strdup("");
    else s = tmc_strdup(def); /* .get(key, default) */
    json_free(raw);
    char *st = tmc_strip(s);
    free(s);
    char *lo = tmc_lower(st);
    free(st);
    bool hit = false;
    for (size_t i = 0; i < n_allowed; i++)
        if (!strcmp(lo, allowed[i])) { hit = true; break; }
    json_t *out = json_string(hit ? lo : def);
    free(lo);
    return out;
}

/* PoP: tmc_display_word @ tui_gateway/methods_config.py:_display_word */
static json_t *tmc_display_word(tmc_ctx_t *ctx, const json_t *args) {
    const char *key = json_get_str(args, "key", "");
    const char *def = json_get_str(args, "default", "");
    const json_t *allowed = json_obj_get(args, "allowed");
    size_t n = (allowed && allowed->type == JSON_ARRAY) ? json_len(allowed) : 0;
    const char **arr = malloc((n + 1) * sizeof(char *));
    if (!arr) return NULL;
    for (size_t i = 0; i < n; i++) {
        const char *s = tmc_str(json_get(allowed, i));
        arr[i] = s ? s : "";
    }
    json_t *out = tmc_display_word_v(ctx, key, def, arr, n);
    free(arr);
    return out;
}

/* -- config getters (named) -- */

typedef json_t *(*tmc_getter_t)(tmc_ctx_t *, const json_t *, const char *);

/* PoP: tmc_cfg_get_provider @ tui_gateway/methods_config.py:_cfg_get_provider */
static json_t *tmc_cfg_get_provider(tmc_ctx_t *ctx, const json_t *params, const char *home) {
    (void)params; (void)home;
    json_t *model = tmc_call(ctx, "resolve_model", tmc_args0());
    if (tmc_is_raise(model)) return model;
    const char *ms = tmc_str(model);
    if (!ms) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'split'", tmc_type_name(model));
        json_free(model);
        return tmc_err_spec("AttributeError", msg);
    }
    const char *slash = strchr(ms, '/');
    json_t *out = json_object();
    json_set(out, "model", model); /* adopted */
    if (slash) {
        size_t hn = (size_t)(slash - ms);
        char *head = malloc(hn + 1);
        if (head) { memcpy(head, ms, hn); head[hn] = '\0'; }
        json_t *a = tmc_args0();
        tmc_asets(a, "provider", head ? head : "");
        free(head);
        json_t *np = tmc_call(ctx, "normalize_provider", a);
        if (tmc_is_raise(np)) { json_free(out); return np; }
        json_set(out, "provider", np);
    } else {
        json_set_hdr(out, "provider", "unknown");
    }
    json_t *lap = tmc_call(ctx, "list_available_providers", tmc_args0());
    if (tmc_is_raise(lap)) { json_free(out); return lap; }
    json_set(out, "providers", lap);
    return out;
}

/* PoP: tmc_cfg_get_project @ tui_gateway/methods_config.py:_cfg_get_project */
static json_t *tmc_cfg_get_project(tmc_ctx_t *ctx, const json_t *params, const char *home) {
    (void)home;
    if (!params || params->type != JSON_OBJECT) return tmc_get_error(params);
    const json_t *cwd_v = json_obj_get(params, "cwd");
    const json_t *raw_src = NULL;
    json_t *cfg = NULL;
    if (tmc_truthy(cwd_v)) {
        raw_src = cwd_v; /* explicit pick short-circuits the load_cfg chain */
    } else {
        cfg = tmc_call(ctx, "load_cfg", tmc_args0());
        if (tmc_is_raise(cfg)) return cfg;
        if (!cfg || cfg->type != JSON_OBJECT) {
            json_t *e = tmc_get_error(cfg);
            json_free(cfg);
            return e;
        }
        const json_t *term = json_obj_get(cfg, "terminal");
        const json_t *term_obj = tmc_truthy(term) ? term : NULL;
        if (term_obj && term_obj->type != JSON_OBJECT) {
            json_t *e = tmc_get_error(term_obj);
            json_free(cfg);
            return e;
        }
        const json_t *cwd2 = term_obj ? json_obj_get(term_obj, "cwd") : NULL;
        raw_src = tmc_truthy(cwd2) ? cwd2 : NULL;
    }
    char *s = raw_src ? tmc_pystr(raw_src) : tmc_strdup("");
    char *raw = tmc_strip(s);
    free(s);
    json_free(cfg);
    /* _completion_cwd({"cwd": raw, "cwd_explicit": ..., "profile": ...}) */
    json_t *cp = json_object();
    tmc_asets(cp, "cwd", raw);
    tmc_asetb(cp, "cwd_explicit", tmc_truthy(json_obj_get(params, "cwd")));
    tmc_aset(cp, "profile", json_obj_get(params, "profile"));
    free(raw);
    json_t *a = tmc_args0();
    json_set(a, "params", cp); /* adopted */
    json_t *cwd = tmc_call(ctx, "completion_cwd", a);
    if (tmc_is_raise(cwd)) return cwd;
    json_t *ba = tmc_args0();
    tmc_aset(ba, "cwd", cwd);
    json_t *br = tmc_call(ctx, "git_branch", ba);
    if (tmc_is_raise(br)) { json_free(cwd); return br; }
    json_t *out = json_object();
    json_set(out, "cwd", cwd);
    json_set(out, "branch", br);
    return out;
}

/* PoP: tmc_cfg_get_personality @ tui_gateway/methods_config.py:_cfg_get_personality */
static json_t *tmc_cfg_get_personality(tmc_ctx_t *ctx, const json_t *params, const char *home) {
    (void)params; (void)home;
    json_t *cfg = tmc_call(ctx, "load_cfg", tmc_args0());
    if (tmc_is_raise(cfg)) return cfg;
    json_t *a = tmc_args0();
    tmc_aset(a, "cfg", cfg);
    json_t *apn = tmc_call(ctx, "active_personality_name", a);
    json_free(cfg);
    if (tmc_is_raise(apn)) return apn;
    json_t *val;
    if (tmc_truthy(apn)) val = apn;
    else { json_free(apn); val = json_string("none"); }
    json_t *out = json_object();
    json_set(out, "value", val);
    return out;
}

/* PoP: tmc_cfg_get_reasoning @ tui_gateway/methods_config.py:_cfg_get_reasoning */
static json_t *tmc_cfg_get_reasoning(tmc_ctx_t *ctx, const json_t *params, const char *home) {
    (void)home;
    if (!params || params->type != JSON_OBJECT) return tmc_get_error(params);
    json_t *cfg = tmc_call(ctx, "load_cfg", tmc_args0());
    if (tmc_is_raise(cfg)) return cfg;
    const json_t *sid_v = json_obj_get(params, "session_id");
    json_t *a = tmc_args0();
    tmc_aset(a, "session_id", sid_v ? sid_v : json_string(""));
    json_t *sess = tmc_call(ctx, "sessions_get", a);
    if (tmc_is_raise(sess)) { json_free(cfg); return sess; }
    const json_t *session = tmc_truthy(sess) ? sess : NULL;
    const json_t *rc = NULL;
    json_t *rc_owned = NULL;
    json_t *err = NULL;
    char *effort = NULL;
    if (session) {
        if (session->type != JSON_OBJECT) { err = tmc_get_error(session); goto cleanup; }
        rc = json_obj_get(session, "create_reasoning_override");
        if (!(rc && rc->type == JSON_OBJECT)) {
            const json_t *agent = json_obj_get(session, "agent");
            a = tmc_args0();
            tmc_aset(a, "agent", agent);
            json_t *srv = tmc_call(ctx, "session_runtime_view", a);
            if (tmc_is_raise(srv)) { err = srv; goto cleanup; }
            const json_t *rcv = (srv && srv->type == JSON_OBJECT)
                ? json_obj_get(srv, "reasoning_config") : NULL;
            rc_owned = rcv ? json_copy(rcv) : NULL;
            json_free(srv);
            rc = rc_owned;
        }
    }
    if (rc && rc->type == JSON_OBJECT) {
        const json_t *en = json_obj_get(rc, "enabled");
        bool enabled = !(en && en->type == JSON_BOOL && !en->bool_val); /* is not False */
        if (enabled) {
            const json_t *ef = json_obj_get(rc, "effort");
            effort = tmc_truthy(ef) ? tmc_pystr(ef) : tmc_strdup("medium");
        } else {
            effort = tmc_strdup("none");
        }
    } else {
        if (!cfg || cfg->type != JSON_OBJECT) { err = tmc_get_error(cfg); goto cleanup; }
        const json_t *ag = json_obj_get(cfg, "agent");
        const json_t *ag_obj = tmc_truthy(ag) ? ag : NULL;
        if (ag_obj && ag_obj->type != JSON_OBJECT) { err = tmc_get_error(ag_obj); goto cleanup; }
        const json_t *raw_effort = ag_obj ? json_obj_get(ag_obj, "reasoning_effort") : NULL;
        json_t *re_owned = NULL;
        if (!raw_effort) { re_owned = json_string(""); raw_effort = re_owned; }
        if (raw_effort->type == JSON_OBJECT) {
            a = tmc_args0();
            tmc_aset(a, "raw", raw_effort);
            json_t *parsed = tmc_call(ctx, "parse_reasoning_effort", a);
            if (tmc_is_raise(parsed)) { err = parsed; json_free(re_owned); goto cleanup; }
            const json_t *p = tmc_truthy(parsed) ? parsed : NULL;
            if (p && p->type != JSON_OBJECT) {
                err = tmc_get_error(p);
                json_free(parsed);
                json_free(re_owned);
                goto cleanup;
            }
            const json_t *en = p ? json_obj_get(p, "enabled") : NULL;
            json_free(re_owned);
            if (en && en->type == JSON_BOOL && !en->bool_val) {
                re_owned = json_bool(false);
            } else {
                const json_t *ef2 = p ? json_obj_get(p, "effort") : NULL;
                re_owned = ef2 ? json_copy(ef2) : NULL;
            }
            raw_effort = re_owned;
            json_free(parsed);
        }
        /* "none" if raw_effort is False else str(raw_effort or "medium") */
        if (raw_effort && raw_effort->type == JSON_BOOL && !raw_effort->bool_val)
            effort = tmc_strdup("none");
        else
            effort = tmc_truthy(raw_effort) ? tmc_pystr(raw_effort) : tmc_strdup("medium");
        json_free(re_owned);
    }
    if (!cfg || cfg->type != JSON_OBJECT) { err = tmc_get_error(cfg); goto cleanup; }
    const json_t *disp = json_obj_get(cfg, "display");
    const json_t *disp_obj = tmc_truthy(disp) ? disp : NULL;
    if (disp_obj && disp_obj->type != JSON_OBJECT) { err = tmc_get_error(disp_obj); goto cleanup; }
    const json_t *sr = disp_obj ? json_obj_get(disp_obj, "show_reasoning") : NULL;
    bool show = sr ? tmc_truthy(sr) : true; /* .get("show_reasoning", True) */
    json_t *out = json_object();
    json_set_hdr(out, "value", effort);
    json_set_hdr(out, "display", show ? "show" : "hide");
    free(effort);
    json_free(rc_owned);
    json_free(sess);
    json_free(cfg);
    return out;
cleanup:
    free(effort);
    json_free(rc_owned);
    json_free(sess);
    json_free(cfg);
    return err;
}

/* PoP: tmc_cfg_get_fast @ tui_gateway/methods_config.py:_cfg_get_fast */
static json_t *tmc_cfg_get_fast(tmc_ctx_t *ctx, const json_t *params, const char *home) {
    (void)home;
    if (!params || params->type != JSON_OBJECT) return tmc_get_error(params);
    const json_t *sid_v = json_obj_get(params, "session_id");
    json_t *a = tmc_args0();
    tmc_aset(a, "session_id", sid_v ? sid_v : json_string(""));
    json_t *sess = tmc_call(ctx, "sessions_get", a);
    if (tmc_is_raise(sess)) return sess;
    const json_t *session = tmc_truthy(sess) ? sess : NULL;
    const json_t *tier = NULL;
    json_t *tier_owned = NULL;
    json_t *err = NULL;
    if (session) {
        if (session->type != JSON_OBJECT) { err = tmc_get_error(session); goto cleanup; }
        const json_t *agent = json_obj_get(session, "agent");
        if (agent && agent->type != JSON_NULL) { /* agent is not None */
            tier = (agent->type == JSON_OBJECT) ? json_obj_get(agent, "service_tier") : NULL;
        } else {
            tier = json_obj_get(session, "create_service_tier_override");
        }
    }
    if (!tier || tier->type == JSON_NULL) { /* tier is None */
        tier_owned = tmc_call(ctx, "load_service_tier", tmc_args0());
        if (tmc_is_raise(tier_owned)) { err = tier_owned; tier_owned = NULL; goto cleanup; }
        tier = tier_owned;
    }
    if (tier && (tier->type == JSON_OBJECT || tier->type == JSON_ARRAY)) {
        err = tmc_unhashable_error(tier);
        goto cleanup;
    }
    const char *ts = tmc_str(tier);
    const char *val = "normal";
    if (ts && !strcmp(ts, "priority")) val = "fast";
    else if (ts && !strcmp(ts, "ultrafast")) val = "ultrafast";
    json_t *out = json_object();
    json_set_hdr(out, "value", val);
    json_free(tier_owned);
    json_free(sess);
    return out;
cleanup:
    json_free(tier_owned);
    json_free(sess);
    return err;
}

/* PoP: tmc_cfg_get_thinking_mode @ tui_gateway/methods_config.py:_cfg_get_thinking_mode */
static json_t *tmc_cfg_get_thinking_mode(tmc_ctx_t *ctx, const json_t *params, const char *home) {
    (void)params; (void)home;
    json_t *raw = tmc_display_word_v(ctx, "thinking_mode", "", TMC_THINKING_MODES, 3);
    if (tmc_is_raise(raw)) return raw;
    const char *rs = tmc_str(raw);
    if (!rs || !rs[0]) { /* legacy details_mode fallback */
        json_free(raw);
        json_t *dw = tmc_display_word_v(ctx, "details_mode", "collapsed", TMC_DETAIL_MODES, 3);
        if (tmc_is_raise(dw)) return dw;
        const char *ds = tmc_str(dw);
        raw = json_string(ds && !strcmp(ds, "expanded") ? "full" : "collapsed");
        json_free(dw);
    }
    json_t *out = json_object();
    json_set(out, "value", raw); /* adopted */
    return out;
}

/* PoP: tmc_cfg_get_mtime @ tui_gateway/methods_config.py:_cfg_get_mtime */
static json_t *tmc_cfg_get_mtime(tmc_ctx_t *ctx, const json_t *params, const char *home) {
    (void)params;
    char *path = tmc_path_join(home, "config.yaml");
    json_t *a = tmc_args0();
    tmc_asets(a, "path", path);
    free(path);
    json_t *cv = tmc_call(ctx, "config_version_or_none", a);
    if (tmc_is_raise(cv)) {
        if (tmc_is_base_exc(tmc_raise_kind(cv))) return cv;
        json_free(cv);
        goto mtime0;
    }
    a = tmc_args0();
    tmc_aset(a, "version", cv);
    json_t *vt = tmc_call(ctx, "version_token", a);
    json_free(cv);
    if (tmc_is_raise(vt)) {
        if (tmc_is_base_exc(tmc_raise_kind(vt))) return vt;
        json_free(vt);
        goto mtime0;
    }
    /* _compute_mcp_rev() is OUTSIDE the try -- its raises propagate. */
    json_t *mcp = tmc_call(ctx, "compute_mcp_rev", tmc_args0());
    if (tmc_is_raise(mcp)) { json_free(vt); return mcp; }
    json_t *out = json_object();
    json_set(out, "mtime", vt);
    json_set(out, "mcp_rev", mcp);
    return out;
mtime0:
    json_t *z = json_object();
    json_set(z, "mtime", json_number(0));
    return z;
}

/* -- _CONFIG_GETTERS lambdas (internal helpers; no PoP -- lambdas unnamed) -- */

/* "profile": {"home": str(_hermes_home), "display": _display_hermes_home()} */
static json_t *tmc_get_profile(tmc_ctx_t *ctx, const json_t *params, const char *home) {
    (void)params;
    json_t *out = json_object();
    json_set_hdr(out, "home", home);
    json_t *d = tmc_call(ctx, "display_hermes_home", tmc_args0());
    if (tmc_is_raise(d)) { json_free(out); return d; }
    json_set(out, "display", d);
    return out;
}

/* "full": {"config": _load_cfg()} */
static json_t *tmc_get_full(tmc_ctx_t *ctx, const json_t *params, const char *home) {
    (void)params; (void)home;
    json_t *cfg = tmc_call(ctx, "load_cfg", tmc_args0());
    if (tmc_is_raise(cfg)) return cfg;
    json_t *out = json_object();
    json_set(out, "config", cfg);
    return out;
}

/* "prompt": {"prompt": _load_cfg().get("custom_prompt", "")} */
static json_t *tmc_get_prompt(tmc_ctx_t *ctx, const json_t *params, const char *home) {
    (void)params; (void)home;
    json_t *cfg = tmc_call(ctx, "load_cfg", tmc_args0());
    if (tmc_is_raise(cfg)) return cfg;
    if (!cfg || cfg->type != JSON_OBJECT) {
        json_t *e = tmc_get_error(cfg);
        json_free(cfg);
        return e;
    }
    const json_t *v = json_obj_get(cfg, "custom_prompt");
    json_t *out = json_object();
    json_set(out, "prompt", v ? json_copy(v) : json_string(""));
    json_free(cfg);
    return out;
}

/* "skin": {"value": _display_raw().get("skin", "default")} */
static json_t *tmc_get_skin(tmc_ctx_t *ctx, const json_t *params, const char *home) {
    (void)params; (void)home;
    json_t *raw = tmc_display_raw_v(ctx);
    if (tmc_is_raise(raw)) return raw;
    const json_t *v = json_obj_get(raw, "skin");
    json_t *out = json_object();
    json_set(out, "value", v ? json_copy(v) : json_string("default"));
    json_free(raw);
    return out;
}

/* "indicator": _display_word("tui_status_indicator", DEFAULT_INDICATOR_STYLE,
 * INDICATOR_STYLES) */
static json_t *tmc_get_indicator(tmc_ctx_t *ctx, const json_t *params, const char *home) {
    (void)params; (void)home;
    json_t *w = tmc_display_word_v(ctx, "tui_status_indicator", "kaomoji",
                                   TMC_INDICATOR_STYLES, 4);
    if (tmc_is_raise(w)) return w;
    json_t *out = json_object();
    json_set(out, "value", w);
    return out;
}

/* "daybreak": (_load_cfg().get("agent") or {}).get("daybreak") is True */
static json_t *tmc_get_daybreak(tmc_ctx_t *ctx, const json_t *params, const char *home) {
    (void)params; (void)home;
    json_t *cfg = tmc_call(ctx, "load_cfg", tmc_args0());
    if (tmc_is_raise(cfg)) return cfg;
    if (!cfg || cfg->type != JSON_OBJECT) {
        json_t *e = tmc_get_error(cfg);
        json_free(cfg);
        return e;
    }
    const json_t *ag = json_obj_get(cfg, "agent");
    const json_t *ag_obj = tmc_truthy(ag) ? ag : NULL;
    if (ag_obj && ag_obj->type != JSON_OBJECT) {
        json_t *e = tmc_get_error(ag_obj);
        json_free(cfg);
        return e;
    }
    const json_t *db = ag_obj ? json_obj_get(ag_obj, "daybreak") : NULL;
    bool val = db && db->type == JSON_BOOL && db->bool_val; /* is True */
    json_t *out = json_object();
    json_set(out, "value", json_bool(val));
    json_free(cfg);
    return out;
}

/* "busy": {"value": _load_busy_input_mode()} */
static json_t *tmc_get_busy(tmc_ctx_t *ctx, const json_t *params, const char *home) {
    (void)params; (void)home;
    json_t *r = tmc_call(ctx, "load_busy_input_mode", tmc_args0());
    if (tmc_is_raise(r)) return r;
    json_t *out = json_object();
    json_set(out, "value", r);
    return out;
}

/* "approval_mode" / "approvals.mode": {"value": _load_approval_mode()} */
static json_t *tmc_get_approval_mode(tmc_ctx_t *ctx, const json_t *params, const char *home) {
    (void)params; (void)home;
    json_t *r = tmc_call(ctx, "load_approval_mode", tmc_args0());
    if (tmc_is_raise(r)) return r;
    json_t *out = json_object();
    json_set(out, "value", r);
    return out;
}

/* "details_mode": _display_word("details_mode", "collapsed", _DETAIL_MODES) */
static json_t *tmc_get_details_mode(tmc_ctx_t *ctx, const json_t *params, const char *home) {
    (void)params; (void)home;
    json_t *w = tmc_display_word_v(ctx, "details_mode", "collapsed", TMC_DETAIL_MODES, 3);
    if (tmc_is_raise(w)) return w;
    json_t *out = json_object();
    json_set(out, "value", w);
    return out;
}

/* "density": "on" if bool(_display_raw().get("tui_compact", False)) else "off" */
static json_t *tmc_get_density(tmc_ctx_t *ctx, const json_t *params, const char *home) {
    (void)params; (void)home;
    json_t *raw = tmc_display_raw_v(ctx);
    if (tmc_is_raise(raw)) return raw;
    const json_t *tc = json_obj_get(raw, "tui_compact");
    bool val = tmc_truthy(tc);
    json_free(raw);
    json_t *out = json_object();
    json_set_hdr(out, "value", val ? "on" : "off");
    return out;
}

/* "theme": _display_word("tui_theme", "auto", {"auto","light","dark"}) */
static json_t *tmc_get_theme(tmc_ctx_t *ctx, const json_t *params, const char *home) {
    (void)params; (void)home;
    json_t *w = tmc_display_word_v(ctx, "tui_theme", "auto", TMC_THEMES, 3);
    if (tmc_is_raise(w)) return w;
    json_t *out = json_object();
    json_set(out, "value", w);
    return out;
}

/* "statusbar": _coerce_statusbar(_display_cfg().get("tui_statusbar", "top")) */
static json_t *tmc_get_statusbar(tmc_ctx_t *ctx, const json_t *params, const char *home) {
    (void)params; (void)home;
    json_t *dc = tmc_call(ctx, "display_cfg", tmc_args0());
    if (tmc_is_raise(dc)) return dc;
    if (!dc || dc->type != JSON_OBJECT) {
        json_t *e = tmc_get_error(dc);
        json_free(dc);
        return e;
    }
    const json_t *sb = json_obj_get(dc, "tui_statusbar");
    json_t *a = tmc_args0();
    if (sb) tmc_aset(a, "raw", sb);
    else tmc_asets(a, "raw", "top");
    json_free(dc);
    json_t *r = tmc_call(ctx, "coerce_statusbar", a);
    if (tmc_is_raise(r)) return r;
    json_t *out = json_object();
    json_set(out, "value", r);
    return out;
}

/* "focus": {"value": on/off(_display_cfg().get("focus_view", False)),
 *           "tool_progress": _load_tool_progress_mode()} */
static json_t *tmc_get_focus(tmc_ctx_t *ctx, const json_t *params, const char *home) {
    (void)params; (void)home;
    json_t *dc = tmc_call(ctx, "display_cfg", tmc_args0());
    if (tmc_is_raise(dc)) return dc;
    if (!dc || dc->type != JSON_OBJECT) {
        json_t *e = tmc_get_error(dc);
        json_free(dc);
        return e;
    }
    const json_t *fv = json_obj_get(dc, "focus_view");
    bool val = tmc_truthy(fv);
    json_free(dc);
    json_t *tp = tmc_call(ctx, "load_tool_progress_mode", tmc_args0());
    if (tmc_is_raise(tp)) return tp;
    json_t *out = json_object();
    json_set_hdr(out, "value", val ? "on" : "off");
    json_set(out, "tool_progress", tp);
    return out;
}

/* "mouse": _display_mouse_tracking(_load_cfg().get("display")) */
static json_t *tmc_get_mouse(tmc_ctx_t *ctx, const json_t *params, const char *home) {
    (void)params; (void)home;
    json_t *cfg = tmc_call(ctx, "load_cfg", tmc_args0());
    if (tmc_is_raise(cfg)) return cfg;
    if (!cfg || cfg->type != JSON_OBJECT) {
        json_t *e = tmc_get_error(cfg);
        json_free(cfg);
        return e;
    }
    const json_t *disp = json_obj_get(cfg, "display");
    json_t *a = tmc_args0();
    tmc_aset(a, "display", disp);
    json_free(cfg);
    json_t *r = tmc_call(ctx, "display_mouse_tracking", a);
    if (tmc_is_raise(r)) return r;
    json_t *out = json_object();
    json_set(out, "value", r);
    return out;
}

/* key -> getter(params); the live _CONFIG_GETTERS table order. */
typedef struct { const char *key; tmc_getter_t fn; } tmc_getter_entry_t;
static const tmc_getter_entry_t TMC_GETTERS[] = {
    {"provider", tmc_cfg_get_provider},
    {"profile", tmc_get_profile},
    {"project", tmc_cfg_get_project},
    {"full", tmc_get_full},
    {"prompt", tmc_get_prompt},
    {"skin", tmc_get_skin},
    {"indicator", tmc_get_indicator},
    {"personality", tmc_cfg_get_personality},
    {"reasoning", tmc_cfg_get_reasoning},
    {"fast", tmc_cfg_get_fast},
    {"daybreak", tmc_get_daybreak},
    {"busy", tmc_get_busy},
    {"approval_mode", tmc_get_approval_mode},
    {"approvals.mode", tmc_get_approval_mode},
    {"details_mode", tmc_get_details_mode},
    {"thinking_mode", tmc_cfg_get_thinking_mode},
    {"density", tmc_get_density},
    {"theme", tmc_get_theme},
    {"statusbar", tmc_get_statusbar},
    {"focus", tmc_get_focus},
    {"mouse", tmc_get_mouse},
    {"mtime", tmc_cfg_get_mtime},
};
#define TMC_GETTER_COUNT (sizeof(TMC_GETTERS) / sizeof(TMC_GETTERS[0]))

/* _CONFIG_GET_ERR = {"provider": 5013, "approval_mode": 5001, "approvals.mode": 5001} */
static int tmc_get_err_code(const char *key) {
    if (!strcmp(key, "provider")) return 5013;
    if (!strcmp(key, "approval_mode") || !strcmp(key, "approvals.mode")) return 5001;
    return 0;
}

static const tmc_getter_entry_t *tmc_find_getter(const json_t *key) {
    const char *ks = tmc_str(key);
    if (!ks) return NULL; /* non-str hashables never match a str key */
    for (size_t i = 0; i < TMC_GETTER_COUNT; i++)
        if (!strcmp(ks, TMC_GETTERS[i].key)) return &TMC_GETTERS[i];
    return NULL;
}

/* Direct getter-table drive (the lambdas + named getters, one dispatch). */
static json_t *tmc_config_getter(tmc_ctx_t *ctx, const json_t *args) {
    const json_t *key = json_obj_get(args, "key");
    const json_t *params = json_obj_get(args, "params");
    const char *home = json_get_str(args, "hermes_home", "");
    if (key && (key->type == JSON_OBJECT || key->type == JSON_ARRAY))
        return tmc_unhashable_error(key);
    const tmc_getter_entry_t *ge = tmc_find_getter(key);
    if (!ge) {
        char *ks = tmc_pystr(key);
        char msg[256];
        snprintf(msg, sizeof msg, "'%s'", ks ? ks : "");
        free(ks);
        return tmc_err_spec("KeyError", msg);
    }
    return ge->fn(ctx, params, home);
}

/* PoP: tmc_config_get @ tui_gateway/methods_config.py:_ */
/* config.get handler (decorated body driven bare). */
static json_t *tmc_config_get(tmc_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    const char *home = json_get_str(args, "hermes_home", "");
    if (!params || params->type != JSON_OBJECT) return tmc_get_error(params);
    const json_t *key_v = json_obj_get(params, "key");
    json_t *key_default = NULL;
    if (!key_v) key_v = key_default = json_string(""); /* .get("key", "") */
    if (key_v->type == JSON_OBJECT || key_v->type == JSON_ARRAY) {
        json_t *e = tmc_unhashable_error(key_v);
        json_free(key_default);
        return e;
    }
    const tmc_getter_entry_t *ge = tmc_find_getter(key_v);
    if (!ge) {
        char *ks = tmc_pystr(key_v);
        char msg[512];
        snprintf(msg, sizeof msg, "unknown config key: %s", ks ? ks : "");
        free(ks);
        json_free(key_default);
        return tmc_err(ctx, rid, 4002, msg);
    }
    json_free(key_default);
    json_t *r = ge->fn(ctx, params, home);
    int err_code = tmc_get_err_code(ge->key);
    if (tmc_is_raise(r)) {
        if (tmc_is_base_exc(tmc_raise_kind(r))) return r;
        if (!err_code) return r; /* key not in _CONFIG_GET_ERR: re-raise */
        char *msg = tmc_strdup(json_get_str(r, "message", ""));
        json_free(r);
        json_t *e = tmc_err(ctx, rid, err_code, msg);
        free(msg);
        return e;
    }
    /* _ok is INSIDE the try -- its Exception raises hit the same handler. */
    json_t *ok = tmc_ok(ctx, rid, r); /* r adopted */
    if (tmc_is_raise(ok)) {
        if (tmc_is_base_exc(tmc_raise_kind(ok))) return ok;
        if (!err_code) return ok;
        char *msg = tmc_strdup(json_get_str(ok, "message", ""));
        json_free(ok);
        json_t *e = tmc_err(ctx, rid, err_code, msg);
        free(msg);
        return e;
    }
    return ok;
}

/* -- projects / repo discovery -- */

/* pdb.reconcile_discovered_repos_policy(conn, policy_key,
 *     preserve_unversioned=_repo_discovery_policy_is_default(policy))
 * The kwarg seam fires BEFORE the call seam. */
static json_t *tmc_reconcile_repo_discovery_v(tmc_ctx_t *ctx, const json_t *conn,
                                              const json_t *policy, const json_t *policy_key) {
    json_t *a = tmc_args0();
    tmc_aset(a, "policy", policy);
    json_t *pu = tmc_call(ctx, "repo_discovery_policy_is_default", a);
    if (tmc_is_raise(pu)) return pu;
    a = tmc_args0();
    tmc_aset(a, "conn", conn);
    tmc_aset(a, "policy_key", policy_key);
    tmc_aset(a, "preserve_unversioned", pu);
    json_free(pu);
    return tmc_call(ctx, "reconcile_discovered_repos_policy", a);
}

/* PoP: tmc_reconcile_repo_discovery @ tui_gateway/methods_config.py:_reconcile_repo_discovery */
static json_t *tmc_reconcile_repo_discovery(tmc_ctx_t *ctx, const json_t *args) {
    return tmc_reconcile_repo_discovery_v(ctx, json_obj_get(args, "conn"),
                                          json_obj_get(args, "policy"),
                                          json_obj_get(args, "policy_key"));
}

/* _build_project_tree(db, **kwargs) + stamp_profile(tree["projects"],
 * _response_profile_name(params.get("profile"))). Returns [tree, active_id]. */
static json_t *tmc_stamped_project_tree_call(tmc_ctx_t *ctx, const json_t *db,
                                             const json_t *params,
                                             double preview_limit, bool hydrate,
                                             double session_limit, bool include_discovered) {
    json_t *a = tmc_args0();
    tmc_aset(a, "db", db);
    tmc_asetn(a, "preview_limit", preview_limit);
    tmc_asetb(a, "hydrate", hydrate);
    tmc_asetn(a, "session_limit", session_limit);
    tmc_asetb(a, "include_discovered", include_discovered);
    json_t *bt = tmc_call(ctx, "build_project_tree", a);
    if (tmc_is_raise(bt)) return bt;
    json_t *tree = NULL, *active = NULL;
    json_t *up = tmc_unpack2(bt, &tree, &active);
    json_free(bt);
    if (up) return up;
    json_t *raise = NULL;
    const json_t *projects = tmc_subscript(tree, "projects", &raise);
    if (raise) { json_free(tree); json_free(active); return raise; }
    a = tmc_args0();
    tmc_aset(a, "profile", json_obj_get(params, "profile"));
    json_t *pn = tmc_call(ctx, "response_profile_name", a);
    if (tmc_is_raise(pn)) { json_free(tree); json_free(active); return pn; }
    a = tmc_args0();
    tmc_aset(a, "projects", projects);
    tmc_aset(a, "profile", pn);
    json_free(pn);
    json_t *sp = tmc_call(ctx, "stamp_profile", a);
    if (tmc_is_raise(sp)) { json_free(tree); json_free(active); return sp; }
    json_free(sp); /* expression statement -- result discarded */
    json_t *out = json_array();
    json_append(out, tree);   /* adopted */
    json_append(out, active); /* adopted */
    return out;
}

/* PoP: tmc_stamped_project_tree @ tui_gateway/methods_config.py:_stamped_project_tree */
static json_t *tmc_stamped_project_tree(tmc_ctx_t *ctx, const json_t *args) {
    return tmc_stamped_project_tree_call(ctx, json_obj_get(args, "db"),
                                         json_obj_get(args, "params"),
                                         json_get_num(args, "preview_limit", 3),
                                         json_get_bool(args, "hydrate", false),
                                         json_get_num(args, "session_limit", 2000),
                                         json_get_bool(args, "include_discovered", true));
}

/* params.get("scan") and policy["enabled"] -- the subscript is evaluated ONLY
 * when scan is truthy; include_cached=policy["enabled"] subscripts again. */
static json_t *tmc_projects_discover_repos_body(tmc_ctx_t *ctx, const json_t *rid,
                                                const json_t *params) {
    json_t *db = NULL, *policy = NULL, *conn = NULL, *pk = NULL, *rec = NULL;
    json_t *sc = NULL, *repos = NULL, *err = NULL;
    json_t *a = tmc_args0();
    tmc_aset(a, "params", params);
    db = tmc_call(ctx, "profile_db", a);
    if (tmc_is_raise(db)) return db;
    policy = tmc_call(ctx, "repo_discovery_policy", tmc_args0());
    if (tmc_is_raise(policy)) { err = policy; policy = NULL; goto cleanup; }
    conn = tmc_call(ctx, "connect_closing", tmc_args0());
    if (tmc_is_raise(conn)) { err = conn; conn = NULL; goto cleanup; }
    a = tmc_args0();
    tmc_aset(a, "policy", policy);
    pk = tmc_call(ctx, "repo_discovery_policy_key", a);
    if (tmc_is_raise(pk)) { err = pk; pk = NULL; goto cleanup; }
    rec = tmc_reconcile_repo_discovery_v(ctx, conn, policy, pk);
    if (tmc_is_raise(rec)) { err = rec; rec = NULL; goto cleanup; }
    json_free(rec);
    rec = NULL;
    if (!params || params->type != JSON_OBJECT) { err = tmc_get_error(params); goto cleanup; }
    if (tmc_truthy(json_obj_get(params, "scan"))) {
        json_t *raise = NULL;
        const json_t *en = tmc_subscript(policy, "enabled", &raise);
        if (raise) { err = raise; goto cleanup; }
        if (tmc_truthy(en)) {
            a = tmc_args0();
            tmc_aset(a, "conn", conn);
            tmc_aset(a, "policy", policy);
            sc = tmc_call(ctx, "scan_discovered_repos_remote", a);
            if (tmc_is_raise(sc)) { err = sc; sc = NULL; goto cleanup; }
            json_free(sc);
            sc = NULL;
        }
    }
    json_t *raise = NULL;
    const json_t *en2 = tmc_subscript(policy, "enabled", &raise);
    if (raise) { err = raise; goto cleanup; }
    a = tmc_args0();
    tmc_aset(a, "db", db);
    tmc_aset(a, "conn", conn);
    tmc_aset(a, "include_cached", en2);
    repos = tmc_call(ctx, "discover_repos_payload", a);
    if (tmc_is_raise(repos)) { err = repos; repos = NULL; goto cleanup; }
    json_t *payload = json_object();
    json_set(payload, "repos", repos); /* adopted */
    repos = NULL;
    json_set(payload, "discovery_policy", json_copy(policy));
    err = tmc_ok(ctx, rid, payload);
cleanup:
    json_free(db);
    json_free(policy);
    json_free(conn);
    json_free(pk);
    json_free(rec);
    json_free(sc);
    json_free(repos);
    return err;
}

/* PoP: tmc_projects_discover_repos @ tui_gateway/methods_config.py:_ */
static json_t *tmc_projects_discover_repos(tmc_ctx_t *ctx, const json_t *args) {
    return tmc_projects_discover_repos_body(ctx, json_obj_get(args, "rid"),
                                            json_obj_get(args, "params"));
}

/* PoP: tmc_projects_record_repos @ tui_gateway/methods_config.py:_ */
static json_t *tmc_projects_record_repos(tmc_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    json_t *policy = NULL, *pk = NULL, *p2 = NULL, *pk2 = NULL, *acc = NULL;
    json_t *pairs = NULL, *conn = NULL, *rec = NULL, *db = NULL, *repos = NULL;
    json_t *err = NULL;
    bool accepted = false;
    policy = tmc_call(ctx, "repo_discovery_policy", tmc_args0());
    if (tmc_is_raise(policy)) return policy;
    json_t *a = tmc_args0();
    tmc_aset(a, "policy", policy);
    pk = tmc_call(ctx, "repo_discovery_policy_key", a);
    if (tmc_is_raise(pk)) { err = pk; pk = NULL; goto cleanup; }
    if (!params || params->type != JSON_OBJECT) { err = tmc_get_error(params); goto cleanup; }
    const json_t *incoming = json_obj_get(params, "discovery_policy");
    if (incoming && incoming->type == JSON_OBJECT) {
        a = tmc_args0();
        tmc_aset(a, "raw", incoming);
        p2 = tmc_call(ctx, "repo_discovery_policy", a);
        if (tmc_is_raise(p2)) { err = p2; p2 = NULL; goto cleanup; }
        a = tmc_args0();
        tmc_aset(a, "policy", p2);
        pk2 = tmc_call(ctx, "repo_discovery_policy_key", a);
        if (tmc_is_raise(pk2)) { err = pk2; pk2 = NULL; goto cleanup; }
        accepted = tmc_json_eq(pk2, pk);
    } else {
        a = tmc_args0();
        tmc_aset(a, "policy", policy);
        acc = tmc_call(ctx, "repo_discovery_policy_is_default", a);
        if (tmc_is_raise(acc)) { err = acc; acc = NULL; goto cleanup; }
        accepted = tmc_truthy(acc);
    }
    json_t *raise = NULL;
    const json_t *en = tmc_subscript(policy, "enabled", &raise);
    if (raise) { err = raise; goto cleanup; }
    accepted = tmc_truthy(en) && accepted; /* bool(policy["enabled"] and accepted) */
    /* pairs comprehension */
    pairs = json_array();
    const json_t *repos_v = json_obj_get(params, "repos");
    const json_t *iter = tmc_truthy(repos_v) ? repos_v : NULL;
    if (iter) {
        if (iter->type != JSON_ARRAY) { err = tmc_iter_error(iter); goto cleanup; }
        for (size_t i = 0; i < json_len(iter); i++) {
            const json_t *item = json_get(iter, i);
            if (item && item->type == JSON_STRING) {
                json_t *pair = json_array();
                json_append(pair, json_copy(item));
                json_append(pair, json_null());
                json_append(pairs, pair);
            } else if (item && item->type == JSON_OBJECT
                       && tmc_truthy(json_obj_get(item, "root"))) {
                json_t *r2 = NULL;
                const json_t *root = tmc_subscript(item, "root", &r2);
                if (r2) { err = r2; goto cleanup; }
                char *rs = tmc_pystr(root);
                const json_t *label = json_obj_get(item, "label");
                json_t *pair = json_array();
                json_append(pair, json_string(rs ? rs : ""));
                free(rs);
                json_append(pair, label ? json_copy(label) : json_null());
                json_append(pairs, pair);
            }
        }
    }
    conn = tmc_call(ctx, "connect_closing", tmc_args0());
    if (tmc_is_raise(conn)) { err = conn; conn = NULL; goto cleanup; }
    rec = tmc_reconcile_repo_discovery_v(ctx, conn, policy, pk);
    if (tmc_is_raise(rec)) { err = rec; rec = NULL; goto cleanup; }
    json_free(rec);
    rec = NULL;
    if (accepted) {
        a = tmc_args0();
        tmc_aset(a, "conn", conn);
        tmc_aset(a, "pairs", pairs);
        tmc_asetb(a, "replace", true);
        tmc_aset(a, "policy_key", pk);
        rec = tmc_call(ctx, "record_discovered_repos", a);
        if (tmc_is_raise(rec)) { err = rec; rec = NULL; goto cleanup; }
        json_free(rec);
        rec = NULL;
    } else if (!tmc_truthy(en)) {
        a = tmc_args0();
        tmc_aset(a, "conn", conn);
        tmc_aset(a, "policy_key", pk);
        rec = tmc_call(ctx, "clear_discovered_repos", a);
        if (tmc_is_raise(rec)) { err = rec; rec = NULL; goto cleanup; }
        json_free(rec);
        rec = NULL;
    }
    a = tmc_args0();
    tmc_aset(a, "params", params);
    db = tmc_call(ctx, "profile_db", a);
    if (tmc_is_raise(db)) { err = db; db = NULL; goto cleanup; }
    const json_t *en2 = tmc_subscript(policy, "enabled", &raise);
    if (raise) { err = raise; goto cleanup; }
    a = tmc_args0();
    tmc_aset(a, "db", db);
    tmc_aset(a, "include_cached", en2);
    repos = tmc_call(ctx, "discover_repos_payload", a);
    if (tmc_is_raise(repos)) { err = repos; repos = NULL; goto cleanup; }
    json_t *payload = json_object();
    json_set(payload, "repos", repos); /* adopted */
    repos = NULL;
    json_set(payload, "accepted", json_bool(accepted));
    json_set(payload, "discovery_policy", json_copy(policy));
    err = tmc_ok(ctx, rid, payload);
cleanup:
    json_free(policy);
    json_free(pk);
    json_free(p2);
    json_free(pk2);
    json_free(acc);
    json_free(pairs);
    json_free(conn);
    json_free(rec);
    json_free(db);
    json_free(repos);
    return err;
}

/* PoP: tmc_projects_tree @ tui_gateway/methods_config.py:_ */
static json_t *tmc_projects_tree(tmc_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    json_t *a = tmc_args0();
    tmc_aset(a, "params", params);
    json_t *db = tmc_call(ctx, "profile_db", a);
    if (tmc_is_raise(db)) return db;
    if (!db || db->type == JSON_NULL) { /* db is None */
        json_free(db);
        json_t *payload = json_object();
        json_set(payload, "projects", json_array());
        json_set(payload, "active_id", json_null());
        json_set(payload, "scoped_session_ids", json_array());
        return tmc_ok(ctx, rid, payload);
    }
    if (!params || params->type != JSON_OBJECT) {
        json_t *e = tmc_get_error(params);
        json_free(db);
        return e;
    }
    const json_t *pl_v = json_obj_get(params, "preview_limit");
    json_t *pl_src = tmc_truthy(pl_v) ? json_copy(pl_v) : json_number(3);
    json_t *pl = tmc_pyint(pl_src);
    json_free(pl_src);
    if (tmc_is_raise(pl)) { json_free(db); return pl; }
    const json_t *sl_v = json_obj_get(params, "session_limit");
    json_t *sl_src = tmc_truthy(sl_v) ? json_copy(sl_v) : json_number(2000);
    json_t *sl = tmc_pyint(sl_src);
    json_free(sl_src);
    if (tmc_is_raise(sl)) { json_free(pl); json_free(db); return sl; }
    json_t *st = tmc_stamped_project_tree_call(ctx, db, params,
                                               pl->num_val, false, sl->num_val, true);
    json_free(pl);
    json_free(sl);
    if (tmc_is_raise(st)) { json_free(db); return st; }
    json_t *tree = NULL, *active = NULL;
    json_t *up = tmc_unpack2(st, &tree, &active);
    json_free(st);
    if (up) { json_free(db); return up; }
    json_t *raise = NULL;
    const json_t *projects = tmc_subscript(tree, "projects", &raise);
    if (raise) { json_free(tree); json_free(active); json_free(db); return raise; }
    const json_t *scoped = tmc_subscript(tree, "scoped_session_ids", &raise);
    if (raise) { json_free(tree); json_free(active); json_free(db); return raise; }
    json_t *payload = json_object();
    json_set(payload, "projects", json_copy(projects));
    json_set(payload, "active_id", active); /* adopted */
    json_set(payload, "scoped_session_ids", json_copy(scoped));
    json_free(tree);
    json_free(db);
    return tmc_ok(ctx, rid, payload);
}

/* PoP: tmc_projects_project_sessions @ tui_gateway/methods_config.py:_ */
static json_t *tmc_projects_project_sessions(tmc_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    if (!params || params->type != JSON_OBJECT) return tmc_get_error(params);
    const json_t *pid_v = json_obj_get(params, "project_id");
    char *pid = tmc_pystr(tmc_truthy(pid_v) ? pid_v : NULL); /* str(... or "") */
    if (!pid[0]) {
        free(pid);
        return tmc_err(ctx, rid, 5063, "project_id required");
    }
    json_t *a = tmc_args0();
    tmc_aset(a, "params", params);
    json_t *db = tmc_call(ctx, "profile_db", a);
    if (tmc_is_raise(db)) { free(pid); return db; }
    if (!db || db->type == JSON_NULL) {
        json_free(db);
        free(pid);
        json_t *payload = json_object();
        json_set(payload, "project", json_null());
        return tmc_ok(ctx, rid, payload);
    }
    const json_t *sl_v = json_obj_get(params, "session_limit");
    json_t *sl_src = tmc_truthy(sl_v) ? json_copy(sl_v) : json_number(5000);
    json_t *sl = tmc_pyint(sl_src);
    json_free(sl_src);
    if (tmc_is_raise(sl)) { json_free(db); free(pid); return sl; }
    json_t *st = tmc_stamped_project_tree_call(ctx, db, params,
                                               0, true, sl->num_val, false);
    json_free(sl);
    if (tmc_is_raise(st)) { json_free(db); free(pid); return st; }
    json_t *tree = NULL, *active = NULL;
    json_t *up = tmc_unpack2(st, &tree, &active);
    json_free(st);
    json_free(active);
    if (up) { json_free(db); free(pid); return up; }
    json_t *raise = NULL;
    const json_t *projects = tmc_subscript(tree, "projects", &raise);
    if (raise) { json_free(tree); json_free(db); free(pid); return raise; }
    if (!projects || projects->type == JSON_NULL
        || projects->type == JSON_NUMBER || projects->type == JSON_BOOL) {
        json_t *e = tmc_iter_error(projects);
        json_free(tree); json_free(db); free(pid);
        return e;
    }
    /* list elements / str chars / dict keys -- the live generator iterates all
     * three; p["id"] on a char/key raises the string-indices TypeError. */
    json_t *pid_node = json_string(pid);
    json_t *found = NULL;
    size_t pn = projects->type == JSON_STRING ? strlen(projects->str_val ? projects->str_val : "")
                : json_len(projects);
    for (size_t i = 0; i < pn; i++) {
        json_t *p_owned = NULL;
        const json_t *p;
        if (projects->type == JSON_ARRAY) {
            p = json_get(projects, i);
        } else if (projects->type == JSON_STRING) {
            char ch[2] = { projects->str_val[i], '\0' };
            p = p_owned = json_string(ch);
        } else { /* JSON_OBJECT: iterate keys */
            p = p_owned = json_string(projects->c.keys[i]);
        }
        json_t *r2 = NULL;
        const json_t *id_v = tmc_subscript(p, "id", &r2);
        if (r2) {
            json_free(p_owned);
            json_free(pid_node); json_free(tree); json_free(db); free(pid);
            return r2;
        }
        if (tmc_json_eq(id_v, pid_node)) { found = json_copy(p); json_free(p_owned); break; }
        json_free(p_owned);
    }
    json_free(pid_node);
    json_t *payload = json_object();
    json_set(payload, "project", found ? found : json_null());
    json_free(tree);
    json_free(db);
    free(pid);
    return tmc_ok(ctx, rid, payload);
}

/* -- setup readiness single-flight -- */

/* Probe callbacks: void* carries either a borrowed json args object or a
 * tmc_run_probe_args_t (the _readiness_check closure capture). */
typedef json_t *(*tmc_probe_t)(tmc_ctx_t *, const void *);

typedef struct tmc_probe_spec {
    tmc_probe_t fn;
    const void *args;
} tmc_probe_spec_t;

/* the run_probe() callback seam (direct _readiness_share drive) */
static json_t *tmc_cb_run_probe(tmc_ctx_t *ctx, const void *args) {
    (void)args;
    return tmc_call(ctx, "run_probe", tmc_args0());
}

/* the probe(profile, scoped) callback seam (direct _readiness_check drive) */
static json_t *tmc_cb_probe(tmc_ctx_t *ctx, const void *v) {
    const json_t *args = v;
    json_t *a = tmc_args0();
    tmc_asets(a, "profile", json_get_str(args, "profile", ""));
    tmc_aset(a, "scoped", json_obj_get(args, "scoped"));
    return tmc_call(ctx, "probe", a);
}

static const char *tmc_future_id_str(const json_t *marker) {
    if (marker && marker->type == JSON_OBJECT) return json_get_str(marker, "id", "");
    return NULL;
}

/* PoP: tmc_readiness_cleared @ tui_gateway/methods_config.py:_readiness_cleared */
/* The factory + its _clear(future) callback driven as one unit: args carry the
 * key and the future id; the inflight identity check is id equality. */
static json_t *tmc_readiness_cleared(tmc_ctx_t *ctx, const json_t *args) {
    const json_t *key = json_obj_get(args, "key");
    const char *fid = json_get_str(args, "future_id", "");
    /* with _readiness_lock: if _readiness_inflight.get(key) is future: pop */
    long i = tmc_inflight_find(ctx, key);
    if (i >= 0) {
        json_t *arr = tmc_inflight_arr(ctx);
        const json_t *pair = json_get(arr, (size_t)i);
        const char *sid = tmc_str(json_get(pair, 1));
        if (sid && !strcmp(sid, fid)) tmc_inflight_pop(ctx, key);
    }
    json_t *a = tmc_args0();
    tmc_asets(a, "id", fid);
    json_t *c = tmc_call(ctx, "future_cancelled", a);
    if (tmc_is_raise(c)) return c;
    bool cancelled = tmc_truthy(c);
    json_free(c);
    if (cancelled) return NULL; /* short-circuit: exception() never called */
    a = tmc_args0();
    tmc_asets(a, "id", fid);
    json_t *e1 = tmc_call(ctx, "future_exception", a);
    if (ctx->last_raise) return e1; /* the seam itself raised */
    if (!e1 || e1->type == JSON_NULL) { /* exception() is None */
        json_free(e1);
        return NULL;
    }
    /* logger.debug(fmt, key, future.exception()) -- the SECOND exception() call.
     * e1/e2 are exception VALUES ({"__raise__", "message"} envelope or null),
     * not seam raises: the value shape is the projection of a real instance. */
    a = tmc_args0();
    tmc_asets(a, "id", fid);
    json_t *e2 = tmc_call(ctx, "future_exception", a);
    if (ctx->last_raise) { json_free(e1); return e2; }
    char *es;
    if (!e2 || e2->type == JSON_NULL) es = tmc_strdup("None");
    else if (e2->type == JSON_OBJECT) es = tmc_strdup(json_get_str(e2, "message", ""));
    else es = tmc_pystr(e2);
    char *ks = tmc_key_str(key);
    size_t n = strlen(es) + strlen(ks) + 96;
    char *msg = malloc(n);
    if (msg) snprintf(msg, n, "readiness probe %s failed after its callers returned: %s", ks, es);
    free(es);
    free(ks);
    json_free(e1);
    json_free(e2);
    a = tmc_args0();
    tmc_asets(a, "msg", msg ? msg : "");
    free(msg);
    json_t *r = tmc_call(ctx, "logger_debug", a);
    if (tmc_is_raise(r)) return r;
    json_free(r); /* expression statement -- _clear returns None */
    return NULL;
}

/* _readiness_share: owner submits + registers the done callback; a joiner
 * waits min(wait_seconds, _READINESS_JOIN_GRACE_SECONDS). TimeoutError ->
 * logger.warning + _err(5097); every other raise propagates. */
static json_t *tmc_readiness_share_body(tmc_ctx_t *ctx, const json_t *rid,
                                        const json_t *key, double wait_seconds,
                                        const tmc_probe_spec_t *probe) {
    long i = tmc_inflight_find(ctx, key);
    bool owner = (i < 0);
    json_t *fut = NULL;
    const char *fid = NULL;
    if (owner) {
        fut = tmc_call(ctx, "pool_submit", tmc_args0());
        if (tmc_is_raise(fut)) return fut;
        /* SINGLE-THREADED MODEL: the submit double runs the probe synchronously
         * right after the pool_submit event (dict markers only -- a null/non-dict
         * marker returns before the probe); the outcome is stored on the future
         * and future.result re-scripts it (a probe raise never escapes submit --
         * the pool thread would store it). */
        if (fut && fut->type == JSON_OBJECT) {
            json_t *pr = probe->fn(ctx, probe->args);
            json_free(pr);
        }
        json_t *idn = tmc_future_id(fut);
        tmc_inflight_set(ctx, key, idn);
        json_free(idn);
        fid = tmc_future_id_str(fut);
    } else {
        json_t *arr = tmc_inflight_arr(ctx);
        const json_t *pair = json_get(arr, (size_t)i);
        const json_t *stored = json_get(pair, 1);
        if (!stored || stored->type == JSON_NULL) {
            /* future.result on a null handle */
            return tmc_err_spec("AttributeError",
                                "'NoneType' object has no attribute 'result'");
        }
        fid = tmc_str(stored);
    }
    if (owner) {
        /* registered OUTSIDE the lock; cb itself is not invoked here */
        if (!fut || fut->type != JSON_OBJECT) {
            char msg[192];
            snprintf(msg, sizeof msg, "'%s' object has no attribute 'add_done_callback'",
                     tmc_type_name(fut));
            json_free(fut);
            return tmc_err_spec("AttributeError", msg);
        }
        json_t *a = tmc_args0();
        tmc_asets(a, "id", fid ? fid : "");
        json_t *adc = tmc_call(ctx, "add_done_callback", a);
        if (tmc_is_raise(adc)) { json_free(fut); return adc; }
        json_free(adc);
    }
    double timeout = owner ? wait_seconds
                           : (wait_seconds < 0.5 ? wait_seconds : 0.5);
    json_t *a = tmc_args0();
    tmc_asets(a, "id", fid ? fid : "");
    tmc_asetn(a, "timeout", timeout);
    json_t *fr = tmc_call(ctx, "future_result", a);
    if (tmc_is_raise(fr)) {
        const char *kind = tmc_raise_kind(fr);
        if (kind && !strcmp(kind, "TimeoutError")) {
            json_free(fr);
            char *ks = tmc_key_str(key);
            char msg[512];
            snprintf(msg, sizeof msg,
                     "readiness probe %s exceeded its budget (%.1fs); "
                     "it continues in the background", ks, wait_seconds);
            free(ks);
            a = tmc_args0();
            tmc_asets(a, "msg", msg);
            json_t *lw = tmc_call(ctx, "logger_warning", a);
            if (tmc_is_raise(lw)) { json_free(fut); return lw; }
            json_free(lw);
            json_free(fut);
            return tmc_err(ctx, rid, 5097,
                           "readiness check still in progress; retrying next tick");
        }
        json_free(fut);
        return fr;
    }
    json_free(fut);
    return tmc_ok(ctx, rid, fr); /* fr adopted */
}

/* PoP: tmc_readiness_share @ tui_gateway/methods_config.py:_readiness_share */
static json_t *tmc_readiness_share(tmc_ctx_t *ctx, const json_t *args) {
    const tmc_probe_spec_t spec = {tmc_cb_run_probe, NULL};
    return tmc_readiness_share_body(ctx, json_obj_get(args, "rid"),
                                    json_obj_get(args, "key"),
                                    json_get_num(args, "wait_seconds", 4.0), &spec);
}

/* the run_probe closure of _readiness_check: scope CM + inner probe call */
typedef struct tmc_run_probe_args {
    const char *profile;
    const json_t *home;       /* borrowed; NULL/JSON_NULL == None */
    tmc_probe_t inner;
    const json_t *inner_args; /* borrowed extra args (e.g. requested) */
} tmc_run_probe_args_t;

static json_t *tmc_run_probe_closure(tmc_ctx_t *ctx, const void *v) {
    const tmc_run_probe_args_t *rp = v;
    json_t *session = json_object();
    if (rp->home && rp->home->type != JSON_NULL) {
        char *hs = tmc_pystr(rp->home);
        json_set(session, "profile_home", json_string(hs ? hs : ""));
        free(hs);
    } else {
        json_set(session, "profile_home", json_null());
    }
    json_t *a = tmc_args0();
    tmc_aset(a, "session", session);
    json_free(session);
    json_t *sc = tmc_call(ctx, "session_profile_runtime_scope", a);
    if (tmc_is_raise(sc)) return sc;
    if (!sc || sc->type != JSON_OBJECT) { /* null/non-dict CM marker */
        json_t *e = tmc_cm_error(sc);
        json_free(sc);
        return e;
    }
    json_free(sc); /* silent enter/exit */
    json_t *inner = json_object();
    tmc_asets(inner, "profile", rp->profile);
    json_t *scoped = json_object();
    if (rp->profile && rp->profile[0]) json_set_hdr(scoped, "profile", rp->profile);
    json_set(inner, "scoped", scoped);
    if (rp->inner_args && rp->inner_args->type == JSON_OBJECT) {
        for (size_t i = 0; i < rp->inner_args->c.count; i++)
            json_set(inner, rp->inner_args->c.keys[i], json_copy(rp->inner_args->c.items[i]));
    }
    json_t *r = rp->inner(ctx, inner);
    json_free(inner);
    return r;
}

/* _readiness_check: profile gate + home resolution + single-flighted probe. */
static json_t *tmc_readiness_check_body(tmc_ctx_t *ctx, const json_t *rid,
                                        const json_t *params, const char *probe_key,
                                        double wait_seconds, tmc_probe_t inner,
                                        const json_t *inner_args) {
    char *profile = tmc_strdup("");
    if (params && params->type == JSON_OBJECT) {
        const json_t *pv = json_obj_get(params, "profile");
        char *s = tmc_pystr(tmc_truthy(pv) ? pv : NULL);
        free(profile);
        profile = tmc_strip(s);
        free(s);
    }
    json_t *home = NULL;
    if (profile[0]) {
        json_t *a = tmc_args0();
        tmc_asets(a, "profile", profile);
        json_t *pe = tmc_call(ctx, "profile_exists", a);
        if (tmc_is_raise(pe)) { free(profile); return pe; }
        if (!tmc_truthy(pe)) {
            json_free(pe);
            char msg[512];
            snprintf(msg, sizeof msg, "Profile '%s' does not exist on this backend.", profile);
            json_t *payload = json_object();
            json_set(payload, "ok", json_bool(false));
            tmc_aset(payload, "profile", json_obj_get(params, "profile"));
            json_set_hdr(payload, "error", msg);
            free(profile);
            return tmc_ok(ctx, rid, payload);
        }
        json_free(pe);
        a = tmc_args0();
        tmc_asets(a, "profile", profile);
        home = tmc_call(ctx, "profile_home", a);
        if (tmc_is_raise(home)) { free(profile); return home; }
    }
    tmc_run_probe_args_t rp = {profile, home, inner, inner_args};
    tmc_probe_spec_t spec = {tmc_run_probe_closure, &rp};
    json_t *key = json_array();
    json_append(key, json_string(probe_key));
    json_append(key, json_string(profile));
    json_t *r = tmc_readiness_share_body(ctx, rid, key, wait_seconds, &spec);
    json_free(key);
    json_free(home);
    free(profile);
    return r;
}

/* PoP: tmc_readiness_check @ tui_gateway/methods_config.py:_readiness_check */
static json_t *tmc_readiness_check(tmc_ctx_t *ctx, const json_t *args) {
    return tmc_readiness_check_body(ctx, json_obj_get(args, "rid"),
                                    json_obj_get(args, "params"),
                                    json_get_str(args, "probe_key", "status"),
                                    json_get_num(args, "wait_seconds", 4.0),
                                    tmc_cb_probe, NULL);
}

/* setup.status probe body: record / launch-record / live-check branches. */
static json_t *tmc_status_probe(tmc_ctx_t *ctx, const void *v) {
    const json_t *args = v;
    const char *profile = json_get_str(args, "profile", "");
    const json_t *scoped = json_obj_get(args, "scoped");
    json_t *record = NULL, *launch = NULL, *hc = NULL, *ftr = NULL, *ff = NULL;
    json_t *err = NULL, *out = NULL;
    if (!profile[0]) {
        record = tmc_call(ctx, "wait_for_record", tmc_args0());
        if (tmc_is_raise(record)) return record;
    }
    if (!record || record->type == JSON_NULL) { /* record is None */
        if (profile[0]) {
            launch = tmc_call(ctx, "wait_for_record", tmc_args0());
            if (tmc_is_raise(launch)) { err = launch; launch = NULL; goto cleanup; }
        }
        json_t *a = tmc_args0();
        tmc_asetb(a, "strict_profile_scope", profile[0] != '\0');
        hc = tmc_call(ctx, "has_any_provider_configured", a);
        if (tmc_is_raise(hc)) { err = hc; hc = NULL; goto cleanup; }
        out = json_object();
        json_set(out, "provider_configured", json_bool(tmc_truthy(hc)));
        json_free(hc);
        hc = NULL;
        if (launch && launch->type != JSON_NULL) { /* launch is not None */
            json_t *raise = NULL;
            const json_t *fta = tmc_attr(launch, "free_tier_account", false, NULL, &raise);
            if (raise) { err = raise; goto cleanup; }
            json_set(out, "ready", json_bool(true));
            json_set(out, "free_tier_account", json_copy(fta));
            ftr = tmc_call(ctx, "free_tier_route", tmc_args0());
            if (tmc_is_raise(ftr)) { err = ftr; ftr = NULL; goto cleanup; }
            json_set(out, "free_tier_route", ftr);
            ftr = NULL;
        }
        if (scoped && scoped->type == JSON_OBJECT)
            for (size_t i = 0; i < scoped->c.count; i++)
                json_set(out, scoped->c.keys[i], json_copy(scoped->c.items[i]));
        goto cleanup;
    }
    /* record path -- dict literal order: provider_configured, ready,
     * free_tier_account, free_tier_route, other_providers, inference_provider,
     * **failure_fields(), **scoped */
    out = json_object();
    json_t *raise = NULL;
    const json_t *pc = tmc_attr(record, "provider_configured", false, NULL, &raise);
    if (raise) { err = raise; goto cleanup; }
    json_set(out, "provider_configured", json_copy(pc));
    json_set(out, "ready", json_bool(true));
    static const char *const fields[] = {"free_tier_account", "free_tier_route",
                                         "other_providers", "inference_provider"};
    for (size_t i = 0; i < 4; i++) {
        const json_t *fv = tmc_attr(record, fields[i], false, NULL, &raise);
        if (raise) { err = raise; goto cleanup; }
        json_set(out, fields[i], json_copy(fv));
    }
    ff = tmc_call(ctx, "failure_fields", tmc_args0());
    if (tmc_is_raise(ff)) { err = ff; ff = NULL; goto cleanup; }
    if (!ff || ff->type != JSON_OBJECT) { err = tmc_mapping_error(ff); goto cleanup; }
    for (size_t i = 0; i < ff->c.count; i++)
        json_set(out, ff->c.keys[i], json_copy(ff->c.items[i]));
    json_free(ff);
    ff = NULL;
    if (scoped && scoped->type == JSON_OBJECT)
        for (size_t i = 0; i < scoped->c.count; i++)
            json_set(out, scoped->c.keys[i], json_copy(scoped->c.items[i]));
cleanup:
    json_free(record);
    json_free(launch);
    json_free(hc);
    json_free(ftr);
    json_free(ff);
    if (err) { json_free(out); return err; }
    return out;
}

/* setup.runtime_check probe body: startup pin / explicit provider, bedrock
 * gate, credentials gate, welcome-host free-tier flag. */
static json_t *tmc_runtime_probe(tmc_ctx_t *ctx, const void *v) {
    const json_t *args = v;
    const char *profile = json_get_str(args, "profile", "");
    const json_t *scoped = json_obj_get(args, "scoped");
    const json_t *requested = json_obj_get(args, "requested");
    const char *req_s = tmc_str(requested);
    json_t *sr = NULL, *startup_model = NULL, *startup_provider = NULL;
    json_t *model = NULL, *runtime = NULL, *hc = NULL, *cfg = NULL;
    json_t *rc = NULL, *suf = NULL, *wh = NULL;
    json_t *provider = NULL, *blamed = NULL;
    char *source_s = NULL, *cfg_pin = NULL;
    json_t *err = NULL, *out = NULL;
    sr = tmc_call(ctx, "resolve_startup_runtime", tmc_args0());
    if (tmc_is_raise(sr)) return sr;
    json_t *up = tmc_unpack2(sr, &startup_model, &startup_provider);
    json_free(sr);
    if (up) return up;
    if (req_s && req_s[0]) {
        model = json_copy(startup_model);
        json_t *a = tmc_args0();
        tmc_asets(a, "requested", req_s);
        tmc_aset(a, "target_model", tmc_truthy(startup_model) ? startup_model : NULL);
        runtime = tmc_call(ctx, "resolve_runtime_provider", a);
        if (tmc_is_raise(runtime)) { err = runtime; runtime = NULL; goto cleanup; }
    } else {
        json_t *ar = tmc_call(ctx, "resolve_agent_model_runtime", tmc_args0());
        if (tmc_is_raise(ar)) { err = ar; goto cleanup; }
        up = tmc_unpack2(ar, &model, &runtime);
        json_free(ar);
        if (up) { err = up; goto cleanup; }
    }
    json_t *a = tmc_args0();
    tmc_asetb(a, "strict_profile_scope", profile[0] != '\0');
    hc = tmc_call(ctx, "has_any_provider_configured", a);
    if (tmc_is_raise(hc)) { err = hc; hc = NULL; goto cleanup; }
    bool provider_configured = tmc_truthy(hc);
    json_free(hc);
    hc = NULL;
    if (!runtime || runtime->type != JSON_OBJECT) { err = tmc_get_error(runtime); goto cleanup; }
    const json_t *prov_v = json_obj_get(runtime, "provider");
    provider = tmc_truthy(prov_v) ? json_copy(prov_v) : json_string("provider");
    const json_t *src_v = json_obj_get(runtime, "source");
    source_s = tmc_pystr(tmc_truthy(src_v) ? src_v : NULL); /* str(... or "") */
    cfg = tmc_call(ctx, "load_cfg", tmc_args0());
    if (tmc_is_raise(cfg)) { err = cfg; cfg = NULL; goto cleanup; }
    if (!cfg || cfg->type != JSON_OBJECT) { err = tmc_get_error(cfg); goto cleanup; }
    const json_t *cfg_model = json_obj_get(cfg, "model");
    cfg_pin = tmc_strdup("");
    if (cfg_model && cfg_model->type == JSON_OBJECT) {
        const json_t *cp = json_obj_get(cfg_model, "provider");
        char *s = tmc_pystr(tmc_truthy(cp) ? cp : NULL);
        free(cfg_pin);
        cfg_pin = tmc_strip(s);
        free(s);
    }
    /* pinned = requested or startup_provider or cfg_pin; blamed = pinned or provider */
    if (req_s && req_s[0]) blamed = json_string(req_s);
    else if (tmc_truthy(startup_provider)) blamed = json_copy(startup_provider);
    else if (cfg_pin[0]) blamed = json_string(cfg_pin);
    else blamed = json_copy(provider);
    const char *prov_s = tmc_str(provider);
    bool bedrock = prov_s && !strcmp(prov_s, "bedrock");
    bool src_hit = !strcmp(source_s, "iam-role") || !strcmp(source_s, "aws-sdk-default-chain");
    if (!provider_configured && bedrock && src_hit) {
        out = json_object();
        json_set(out, "ok", json_bool(false));
        json_set(out, "provider", json_copy(blamed));
        json_set(out, "model", json_copy(model));
        json_set(out, "source", json_string(source_s));
        json_set_hdr(out, "error", "No Hermes provider is configured.");
        if (scoped && scoped->type == JSON_OBJECT)
            for (size_t i = 0; i < scoped->c.count; i++)
                json_set(out, scoped->c.keys[i], json_copy(scoped->c.items[i]));
        goto cleanup;
    }
    a = tmc_args0();
    tmc_aset(a, "runtime", runtime);
    rc = tmc_call(ctx, "runtime_has_credentials", a);
    if (tmc_is_raise(rc)) { err = rc; rc = NULL; goto cleanup; }
    if (!tmc_truthy(rc)) {
        a = tmc_args0();
        tmc_aset(a, "provider", blamed);
        suf = tmc_call(ctx, "missing_credentials_suffix", a);
        if (tmc_is_raise(suf)) { err = suf; suf = NULL; goto cleanup; }
        char *bs = tmc_pystr(blamed);
        char *ss = tmc_pystr(suf);
        size_t n = strlen(bs) + strlen(ss) + 64;
        char *msg = malloc(n);
        if (msg) snprintf(msg, n, "No usable credentials found for %s.%s", bs, ss);
        free(bs);
        free(ss);
        out = json_object();
        json_set(out, "ok", json_bool(false));
        json_set(out, "provider", json_copy(blamed));
        json_set(out, "model", json_copy(model));
        tmc_aset(out, "source", src_v); /* runtime.get("source") raw */
        json_set(out, "error", json_string(msg ? msg : ""));
        free(msg);
        if (scoped && scoped->type == JSON_OBJECT)
            for (size_t i = 0; i < scoped->c.count; i++)
                json_set(out, scoped->c.keys[i], json_copy(scoped->c.items[i]));
        goto cleanup;
    }
    json_free(rc);
    rc = NULL;
    /* ok path */
    out = json_object();
    json_set(out, "ok", json_bool(true));
    tmc_aset(out, "provider", prov_v);
    json_set(out, "model", json_copy(model));
    tmc_aset(out, "source", src_v);
    if (prov_s && !strcmp(prov_s, "nous")) {
        a = tmc_args0();
        tmc_aset(a, "base_url", json_obj_get(runtime, "base_url"));
        wh = tmc_call(ctx, "route_is_welcome_host", a);
        if (tmc_is_raise(wh)) { err = wh; wh = NULL; goto cleanup; }
        json_set(out, "free_tier_route", wh);
        wh = NULL;
    } else {
        json_set(out, "free_tier_route", json_bool(false));
    }
    if (scoped && scoped->type == JSON_OBJECT)
        for (size_t i = 0; i < scoped->c.count; i++)
            json_set(out, scoped->c.keys[i], json_copy(scoped->c.items[i]));
cleanup:
    free(source_s);
    free(cfg_pin);
    json_free(startup_model);
    json_free(startup_provider);
    json_free(model);
    json_free(runtime);
    json_free(hc);
    json_free(cfg);
    json_free(rc);
    json_free(suf);
    json_free(wh);
    json_free(provider);
    json_free(blamed);
    if (err) { json_free(out); return err; }
    return out;
}

/* PoP: tmc_setup_status @ tui_gateway/methods_config.py:_ */
static json_t *tmc_setup_status(tmc_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    json_t *r = tmc_readiness_check_body(ctx, rid, params, "status", 12.0,
                                         tmc_status_probe, NULL);
    if (tmc_is_raise(r)) {
        if (tmc_is_base_exc(tmc_raise_kind(r))) return r;
        char *msg = tmc_strdup(json_get_str(r, "message", ""));
        json_free(r);
        json_t *e = tmc_err(ctx, rid, 5016, msg);
        free(msg);
        return e;
    }
    return r;
}

/* PoP: tmc_setup_runtime_check @ tui_gateway/methods_config.py:_ */
static json_t *tmc_setup_runtime_check(tmc_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    json_t *r = NULL;
    char *requested = NULL;
    json_t *inner_args = json_object();
    if (!params || params->type != JSON_OBJECT) {
        r = tmc_get_error(params); /* inside the try -> readiness_failure */
        goto except_path;
    }
    const json_t *pv = json_obj_get(params, "provider");
    char *s = tmc_pystr(tmc_truthy(pv) ? pv : NULL);
    requested = tmc_strip(s);
    free(s);
    if (requested[0]) json_set_hdr(inner_args, "requested", requested);
    char probe_key[512];
    snprintf(probe_key, sizeof probe_key, "runtime:%s", requested);
    r = tmc_readiness_check_body(ctx, rid, params, probe_key, 4.0,
                                 tmc_runtime_probe, inner_args);
    if (!tmc_is_raise(r)) goto done;
except_path:
    if (tmc_is_base_exc(tmc_raise_kind(r))) goto done;
    json_t *a = tmc_args0();
    tmc_asets(a, "type", tmc_raise_kind(r));
    tmc_asets(a, "message", json_get_str(r, "message", ""));
    json_free(r);
    r = tmc_call(ctx, "readiness_failure", a);
    if (tmc_is_raise(r)) goto done;
    r = tmc_ok(ctx, rid, r); /* adopted */
done:
    json_free(inner_args);
    free(requested);
    return r;
}

/* PoP: tmc_share_nous @ tui_gateway/methods_config.py:_ */
static json_t *tmc_share_nous(tmc_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    json_t *bundle = NULL, *red = NULL, *bn = NULL, *res = NULL;
    json_t *err = NULL, *result = NULL;
    if (!params || params->type != JSON_OBJECT) { err = tmc_get_error(params); goto except_path; }
    const json_t *ll_v = json_obj_get(params, "log_lines");
    double log_lines = 200;
    bool is_int = ll_v && (ll_v->type == JSON_BOOL
        || (ll_v->type == JSON_NUMBER && ll_v->num_val == (double)(long long)ll_v->num_val));
    if (is_int) {
        double d = ll_v->type == JSON_BOOL ? (ll_v->bool_val ? 1.0 : 0.0) : ll_v->num_val;
        if (d >= 10 && d <= 2000) log_lines = d;
    }
    json_t *a = tmc_args0();
    tmc_asetn(a, "log_lines", log_lines);
    tmc_asetb(a, "redact", true);
    bundle = tmc_call(ctx, "collect_share_bundle", a);
    if (tmc_is_raise(bundle)) { err = bundle; bundle = NULL; goto except_path; }
    if (!bundle || bundle->type != JSON_OBJECT) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object does not support item assignment",
                 tmc_type_name(bundle));
        err = tmc_err_spec("TypeError", msg);
        goto except_path;
    }
    const json_t *ec = json_obj_get(params, "error_context");
    const char *ecs = tmc_str(ec);
    if (ecs) {
        char *st = tmc_strip(ecs);
        if (st && st[0]) {
            red = tmc_redact_client_text_call(ctx, st, 8000);
            if (tmc_is_raise(red)) { err = red; red = NULL; free(st); goto except_path; }
            json_set(bundle, "error-context.txt", red);
            red = NULL;
        }
        free(st);
    }
    const json_t *extra = json_obj_get(params, "extra_files");
    if (extra && extra->type == JSON_OBJECT) {
        size_t n = extra->c.count < 4 ? extra->c.count : 4;
        for (size_t i = 0; i < n; i++) {
            const char *label = extra->c.keys[i];
            const json_t *text = extra->c.items[i];
            char *safe = tmc_safe_client_label_v(label); /* labels are str keys */
            const char *ts = tmc_str(text);
            bool use = false;
            if (safe && safe[0] && ts) {
                char *st = tmc_strip(ts);
                use = st && st[0];
                free(st);
            }
            if (use) {
                red = tmc_redact_client_text_call(ctx, ts, 524288);
                if (tmc_is_raise(red)) { err = red; red = NULL; free(safe); goto except_path; }
                char key[160];
                snprintf(key, sizeof key, "client/%s", safe);
                json_set(bundle, key, red);
                red = NULL;
            }
            free(safe);
        }
    }
    a = tmc_args0();
    tmc_aset(a, "bundle", bundle);
    tmc_asetb(a, "redact", true);
    bn = tmc_call(ctx, "build_nous_bundle", a);
    if (tmc_is_raise(bn)) { err = bn; bn = NULL; goto except_path; }
    a = tmc_args0();
    tmc_aset(a, "bundle", bn);
    res = tmc_call(ctx, "share_to_nous", a);
    json_free(bn);
    bn = NULL;
    if (tmc_is_raise(res)) { err = res; res = NULL; goto except_path; }
    if (!res || res->type != JSON_OBJECT) { err = tmc_get_error(res); goto except_path; }
    const json_t *v1 = json_obj_get(res, "viewUrl");
    const json_t *view = tmc_truthy(v1) ? v1 : json_obj_get(res, "view_url");
    const json_t *uid = json_obj_get(res, "id");
    if (!tmc_truthy(view) && !tmc_truthy(uid)) {
        json_t *payload = json_object();
        json_set(payload, "ok", json_bool(false));
        json_set_hdr(payload, "error", "upload succeeded but returned no view URL or id");
        result = tmc_ok(ctx, rid, payload);
        if (tmc_is_raise(result)) { err = result; result = NULL; goto except_path; }
        goto cleanup;
    }
    const json_t *e1 = json_obj_get(res, "expiresAt");
    const json_t *expires = tmc_truthy(e1) ? e1 : json_obj_get(res, "expires_at");
    json_t *payload = json_object();
    json_set(payload, "ok", json_bool(true));
    tmc_aset(payload, "view_url", view);
    tmc_aset(payload, "upload_id", uid);
    tmc_aset(payload, "expires_at", expires);
    result = tmc_ok(ctx, rid, payload);
    if (tmc_is_raise(result)) { err = result; result = NULL; goto except_path; }
    goto cleanup;
except_path:
    if (tmc_is_base_exc(tmc_raise_kind(err))) { result = err; err = NULL; goto cleanup; }
    /* redact_debug_support_text(e) -- str(exc) == message; no max_chars */
    a = tmc_args0();
    tmc_asets(a, "text", json_get_str(err, "message", ""));
    json_t *rf = tmc_call(ctx, "redact_debug_support_text", a);
    if (tmc_is_raise(rf)) { result = rf; goto cleanup; } /* propagates */
    json_t *epayload = json_object();
    json_set(epayload, "ok", json_bool(false));
    json_set(epayload, "error", rf); /* adopted */
    result = tmc_ok(ctx, rid, epayload);
cleanup:
    json_free(bundle);
    json_free(red);
    json_free(bn);
    json_free(res);
    json_free(err);
    return result;
}

/* PoP: tmc_projects_handler @ tui_gateway/methods_config.py:_projects_handler */
/* The decorator factory driven as one unit: registration seams fire in
 * Python evaluation order (method(name) BEFORE _profile_scoped(handler)),
 * then the wrapped handler body runs the fn callback seam; an uncaught
 * Exception becomes _err(rid, 5061, str(e)). */
static json_t *tmc_projects_handler(tmc_ctx_t *ctx, const json_t *args) {
    const char *name = json_get_str(args, "name", "");
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    json_t *a = tmc_args0();
    tmc_asets(a, "name", name);
    json_t *m = tmc_call(ctx, "method", a);
    if (tmc_is_raise(m)) return m;
    json_free(m); /* identity decorator marker discarded */
    json_t *ps = tmc_call(ctx, "profile_scoped", tmc_args0());
    if (tmc_is_raise(ps)) return ps;
    json_free(ps);
    a = tmc_args0();
    tmc_aset(a, "rid", rid);
    tmc_aset(a, "params", params);
    json_t *r = tmc_call(ctx, "projects_fn", a);
    if (tmc_is_raise(r)) {
        if (tmc_is_base_exc(tmc_raise_kind(r))) return r;
        char *msg = tmc_strdup(json_get_str(r, "message", ""));
        json_free(r);
        json_t *e = tmc_err(ctx, rid, 5061, msg);
        free(msg);
        return e;
    }
    return r;
}
