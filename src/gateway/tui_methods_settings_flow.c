/*
 * tui_methods_settings_flow.c -- FLOW + leaf port of tui_gateway/methods_config_set.py
 * (hermes-agent): the ``config.set`` settings surface -- one JSON-RPC method
 * dispatched on ``key`` through _CONFIG_SETTERS, plus the shared managed-key
 * refusal, display-section write-back, model-switch stash and the per-key
 * setters (model/fast/busy/verbose/focus/yolo/reasoning/word/toggle/cwd/prompt/
 * personality/skin/display-toggle/daybreak).
 *
 * MODULE RESOLUTION: the task named tui_gateway/methods_settings.py, which does
 * NOT exist in the quarry. The TUI settings surface is the methods_*.py family;
 * the config.set handlers live in tui_gateway/methods_config_set.py. PoP credit
 * is to that REAL defining module (tui_gateway/methods_config_set.py:<fn>).
 *
 * The module's functions are rebound onto server.py's globals at install time
 * (method_ctx.bind_module), so _err / _ok / _emit / _sessions / _load_cfg /
 * _display_cfg / _write_config_key(s) / _save_cfg / the agent_factory +
 * model_switch + personality helpers are bare free variables. ALL of them are
 * seams here (external to methods_config_set.py). _ok / _err / _load_cfg /
 * _display_cfg are CLAIMED by tui_server_flow.c / tui_methods_config_flow.c --
 * seamed, never re-ported.
 *
 * Ports the DECISION SEQUENCES -- branch order, error-code/message composition,
 * seam call order, dict-literal evaluation order, eager dict.get default
 * evaluation and exception paths. Every live external call enters through ONE
 * universal seam: tset_call(ctx, name, args). The harness wires it to a
 * recording stub fed the case's scripted results (in.io); the oracle installs
 * recording stubs emitting the identical event shape {"name","args","ret"} from
 * the SAME scripts.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and tset_-prefixed.
 *
 * Decorated handlers: the @_cfgset_guarded setters (_set_model / _set_yolo /
 * _set_reasoning / _set_prompt / _set_personality / _set_skin) are ported WITH
 * their guard wrapper (tset_guarded_run): ConfigWriteError / SessionNotOwned
 * (and the ManagedKeyError subclass) pass through; every other Exception becomes
 * _err(rid, 5001, str(e)); BaseException-only kinds pass through. The
 * @_profile_scoped / @method("config.set") dispatcher ``def _`` is ported BARE
 * (registry decorators stripped) as tset_h_config_set. _cfgset_guarded itself is
 * ported as a decorator-factory flow driven with a guarded_fn callback seam.
 * The _set_yolo nested ``persist`` closure is ported as tset_set_yolo_persist.
 * _word_setters / _toggle_setters are internal table builders (their lambdas
 * have no portable JSON projection): their spec data + apply-lambda bodies are
 * transcribed INLINE in tset_set_word / tset_set_toggle (no PoP re-claim); the
 * oracle extracts them REAL so the setters compose live.
 *
 * Ported siblings compose LIVE: _word/_raw_word inside the setters; _kv inside
 * _cfgset_model_ok and every setter; _cfgset_model_ok inside _stash;
 * _refuse_managed_keys inside _refuse_managed_model_pick / _write_display_sections
 * / _set_prompt / _set_personality; _refuse_managed_model_pick inside _stash;
 * _emit_session_info inside _emit_all_session_info / _set_fast / _set_reasoning /
 * _set_yolo; _cfgset_await_agent inside _set_model; _set_word inside _set_busy /
 * _set_approval_mode; _write_display_sections inside _set_details_section /
 * _set_reasoning / _set_word(details_mode); the setters inside _h_config_set.
 *
 * Seam name -> live call (args are informational projections):
 *   err                          _err(rid, code, msg)  [server global]
 *   ok                           _ok(rid, result)  [claimed sibling]
 *   emit                         _emit(event, sid, payload)  [server]
 *   session_info                 _session_info(agent, session)  [server]
 *   sessions_get                 _sessions.get(session_id)  [server dict]
 *   sessions_items               list(_sessions.items()) -> [[sid, sess], ...]
 *   wait_agent                   _wait_agent(session, rid)  [server]
 *   load_cfg                     _load_cfg()  [claimed sibling]
 *   load_cfg_raw                 _load_cfg_raw()  [server]
 *   save_cfg                     _save_cfg(cfg)  [server]
 *   display_cfg                  _display_cfg()  [claimed sibling]
 *   write_config_key             _write_config_key(key, value)  [server]
 *   write_config_keys            _write_config_keys(updates)  [server]
 *   write_config_changes         _write_config_changes(sets, drops)  [server]
 *   load_service_tier            _load_service_tier()  [agent_factory]
 *   resolve_model                _resolve_model()  [agent_factory]
 *   load_busy_input_mode         _load_busy_input_mode()  [server]
 *   load_tool_progress_mode      _load_tool_progress_mode()  [agent_factory]
 *   coerce_statusbar             _coerce_statusbar(raw)  [agent_factory]
 *   display_mouse_tracking       _display_mouse_tracking(display)  [agent_factory]
 *   persist_live_session_runtime _persist_live_session_runtime(session)  [server]
 *   start_agent_build            _start_agent_build(sid, session)  [server]
 *   restart_failed_agent_build   _restart_completed_failed_agent_build(sid, session, ready)
 *   session_profile_runtime_scope _session_profile_runtime_scope(session) -> CM
 *                                marker (dict); null/non-dict raises TypeError
 *                                at the live `with` site (enter/exit silent)
 *   apply_model_switch           _apply_model_switch(sid, session, value,
 *                                confirm_expensive_model=, parsed_flags=)
 *   config_model_target          _config_model_target() -> [model, provider]
 *   pending_switch_selection_warning _pending_switch_selection_warning(model, provider, agent)
 *   validate_personality         _validate_personality(value, cfg) -> [pname, prompt]
 *   apply_personality_to_session _apply_personality_to_session(sid, session, prompt, pname)
 *                                -> [history_reset, info]
 *   broadcast_global_event       _broadcast_global_event(event, payload)  [server]
 *   note_skin_broadcast          _note_skin_broadcast()  [change_watcher]
 *   resolve_skin                 resolve_skin()  [change_watcher]
 *   git_branch                   git_probe.branch(cwd)
 *   sess_nowait                  _sess_nowait(params, rid) -> [_, envelope]
 *   session_db                   _session_db(session) -> CM yielding db marker
 *   persistence_for              _persistence_for(db) -> CM yielding persistence
 *   set_yolo                     persistence.sessions.set_yolo(row_id, on)
 *   toggle_session_yolo          toggle_session_yolo(session_key, value, persist=)
 *                                (persist callback passed as marker, NOT invoked
 *                                -- tset_set_yolo_persist has its own cases)
 *   is_truthy_value              utils.is_truthy_value(value)
 *   environ_get                  os.environ.get(key)
 *   environ_set                  os.environ[key] = value
 *   environ_pop                  os.environ.pop(key, None)
 *   expanduser                   os.path.expanduser(raw)
 *   abspath                      os.path.abspath(path)
 *   isdir                        os.path.isdir(path)
 *   get_hermes_home              get_hermes_home() -> path string
 *   path_resolve                 Path(p).resolve() -> path string (monkeypatched
 *                                live so it never hits the real filesystem)
 *   event_wait                   failed_ready.wait(timeout=30.0)
 *   event_is_set                 failed_ready.is_set()
 *   guarded_fn                   the fn(rid, params, key, value, session) callback
 *                                inside _cfgset_guarded
 *   is_key_managed               managed_scope.is_key_managed(key_path)
 *   managed_config_keys          managed_scope.managed_config_keys()
 *   managed_source               hermes_cli.config._managed_source("config.yaml")
 *   read_user_config_raw         hermes_cli.config.read_user_config_raw()
 *   model_selection_config_updates hermes_cli.model_switch.model_selection_config_updates(result, model)
 *   resolve_persist_behavior     hermes_cli.model_switch.resolve_persist_behavior(
 *                                is_global, is_session, is_once=, explicit_provider=)
 *   parse_model_switch_args      hermes_cli.model_switch.parse_model_switch_args(value)
 *                                -> parsed attribute double
 *   parse_reasoning_effort       hermes_constants.parse_reasoning_effort(raw)
 *   supports_file_tooling        hermes_cli.config_backend.supports_file_tooling()
 *   service_tier_word            agent.fast_mode.service_tier_word(tier)
 *   resolve_fast_mode_overrides  hermes_cli.models.resolve_fast_mode_overrides(
 *                                model, provider=, base_url=, tier=)
 *   ultrafast_offered            agent.codex_service_tiers.ultrafast_offered(
 *                                model, provider=, base_url=, access_token=)
 *   normalize_approval_mode      tools.approval_context._normalize_approval_mode(mode)
 *   resolve_focus_arg            hermes_cli.focus_view.resolve_focus_arg(arg, current)
 *                                -> [action, target]
 *   normalize_tool_progress_mode hermes_cli.focus_view.normalize_tool_progress_mode(mode)
 *   persist_personality          hermes_cli.personality.persist_personality(pname)
 *
 * Representation contract (shared with tests/oracle_tui_methods_settings_flow.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)}; BaseException-only
 *   kinds (SystemExit, KeyboardInterrupt, GeneratorExit, CancelledError) PASS
 *   THROUGH every except-Exception site. except Exception uses the FULL builtin
 *   hierarchy table (tset_exc_parent); ConfigWriteError/ManagedKeyError/
 *   SessionNotOwned are a registered family (tset_exc_is_config_write).
 * - The session carrier is ctx.state.session (a raw dict or null). Its "agent"
 *   field is an attribute double: scripted dicts become SimpleNamespaces
 *   (missing attr raises "'types.SimpleNamespace' object has no attribute
 *   '<key>'"); non-dict agents raise the live type-name AttributeError at the
 *   access/write site. Session dicts stay raw dicts (get/subscript/pop).
 * - parsed_flags / parsed model-switch doubles: scripted dicts become attribute
 *   doubles carrying explicit_provider/is_global/is_session/is_once (required
 *   present), is_reset/reasoning_effort (getattr defaults), and model_input
 *   (a property: a scripted model_input_raise {type,message} RAISES on access,
 *   caught by the stash's except Exception). Fixture domain: every directly-read
 *   attribute is present (the double class name never leaks into a message).
 * - agent_ready is an Event marker {"id": ...}; wait/is_set are seams. Identity
 *   (session.get("agent_ready") is failed_ready) is modeled by id equality.
 * - _hermes_home is injected via args.hermes_home (absolute POSIX path); the
 *   oracle drives it as a Path so str() stays POSIX. Path.resolve is a seam.
 * - Tuples project to lists; sets to sorted lists (membership-only sets stay
 *   sets); integral floats to ints; Paths to POSIX strings. Fixture numbers are
 *   ints except event_wait timeout (30.0). str()/strip()/lower() fold the ASCII
 *   corpus only (strip ws set includes \x1c-\x1f). str.format {value}/{raw!r}
 *   error templates are restricted to scalar values (str/int/bool/null).
 * - Seam args drop None-valued keys on both sides (absent-vs-null parity).
 * - Fail-closed: a missing/exhausted script is a RECORDED RuntimeError raise
 *   {"no script for X" / "script exhausted for X"} on both sides.
 * - List-valued seam RESULTS are wrapped one level deeper (bare array == a
 *   per-call queue): sessions_items / config_model_target / validate_personality
 *   / apply_personality_to_session / resolve_focus_arg / model_selection_config_updates.
 *
 * Ownership: tset_call ADOPTS args into the trace event; its return is a fresh
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
typedef struct tset_ctx {
    json_t *state;      /* owned; {"session": carrier}; emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
    bool last_raise;    /* the last tset_call produced a scripted/fail-closed raise */
    bool has_override;  /* dispatcher overrides the session seen by handlers */
    json_t *session_override; /* borrowed; NULL == no session (stale) */
} tset_ctx_t;

/* -- basic helpers -- */
static bool tset_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static const char *tset_raise_kind(const json_t *v) {
    return tset_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}

/* BaseException-only kinds pass through every except-Exception site. */
static bool tset_is_base_exc(const char *kind) {
    return kind && (!strcmp(kind, "SystemExit") || !strcmp(kind, "KeyboardInterrupt")
                    || !strcmp(kind, "GeneratorExit") || !strcmp(kind, "CancelledError"));
}

/* FULL builtin exception hierarchy (parent table). except Exception walks this
 * to BaseException; the BaseException trio stops at BaseException (NOT Exception)
 * so it passes through. ConfigWriteError/ManagedKeyError/SessionNotOwned are a
 * registered family handled by tset_exc_is_config_write / tset_exc_is_kind. */
static const char *tset_exc_parent(const char *kind) {
    if (!kind) return NULL;
    /* BaseException trio -> BaseException ONLY */
    if (!strcmp(kind, "SystemExit") || !strcmp(kind, "KeyboardInterrupt")
        || !strcmp(kind, "GeneratorExit")) return "BaseException";
    if (!strcmp(kind, "CancelledError")) return "BaseException";
    if (!strcmp(kind, "Exception")) return "BaseException";
    if (!strcmp(kind, "BaseException")) return NULL;
    /* Exception direct children */
    if (!strcmp(kind, "RuntimeError") || !strcmp(kind, "ValueError")
        || !strcmp(kind, "TypeError") || !strcmp(kind, "AttributeError")
        || !strcmp(kind, "NameError") || !strcmp(kind, "LookupError")
        || !strcmp(kind, "ArithmeticError") || !strcmp(kind, "OSError")
        || !strcmp(kind, "StopIteration") || !strcmp(kind, "StopAsyncIteration")
        || !strcmp(kind, "AssertionError") || !strcmp(kind, "SyntaxError")
        || !strcmp(kind, "SystemError") || !strcmp(kind, "ImportError")
        || !strcmp(kind, "Warning") || !strcmp(kind, "MemoryError")
        || !strcmp(kind, "BufferError") || !strcmp(kind, "EOFError")
        || !strcmp(kind, "ReferenceError") || !strcmp(kind, "UnboundLocalError")
        || !strcmp(kind, "FloatingPointError") || !strcmp(kind, "OverflowError")
        || !strcmp(kind, "ZeroDivisionError") || !strcmp(kind, "InterruptedError")
        || !strcmp(kind, "ConnectionError") || !strcmp(kind, "BlockingIOError")
        || !strcmp(kind, "ChildProcessError") || !strcmp(kind, "FileExistsError")
        || !strcmp(kind, "FileNotFoundError") || !strcmp(kind, "IsADirectoryError")
        || !strcmp(kind, "NotADirectoryError") || !strcmp(kind, "PermissionError")
        || !strcmp(kind, "ProcessLookupError") || !strcmp(kind, "TimeoutError")
        || !strcmp(kind, "BrokenPipeError") || !strcmp(kind, "ConnectionAbortedError")
        || !strcmp(kind, "ConnectionRefusedError") || !strcmp(kind, "ConnectionResetError")
        || !strcmp(kind, "NotImplementedError") || !strcmp(kind, "RecursionError")
        || !strcmp(kind, "IndentationError") || !strcmp(kind, "TabError")
        || !strcmp(kind, "UnicodeError") || !strcmp(kind, "ModuleNotFoundError")
        || !strcmp(kind, "IndexError") || !strcmp(kind, "KeyError")
        || !strcmp(kind, "UnicodeDecodeError") || !strcmp(kind, "UnicodeEncodeError")
        || !strcmp(kind, "UnicodeTranslateError") || !strcmp(kind, "DeprecationWarning")
        || !strcmp(kind, "PendingDeprecationWarning") || !strcmp(kind, "RuntimeWarning")
        || !strcmp(kind, "SyntaxWarning") || !strcmp(kind, "UserWarning")
        || !strcmp(kind, "FutureWarning") || !strcmp(kind, "ImportWarning")
        || !strcmp(kind, "UnicodeWarning") || !strcmp(kind, "BytesWarning")
        || !strcmp(kind, "ResourceWarning") || !strcmp(kind, "EncodingWarning")
        || !strcmp(kind, "GeneratorExit")) return "Exception";
    /* deeper subclasses */
    if (!strcmp(kind, "NotImplementedError") || !strcmp(kind, "RecursionError"))
        return "RuntimeError";
    if (!strcmp(kind, "ModuleNotFoundError")) return "ImportError";
    if (!strcmp(kind, "IndexError") || !strcmp(kind, "KeyError")) return "LookupError";
    if (!strcmp(kind, "UnboundLocalError")) return "NameError";
    if (!strcmp(kind, "FloatingPointError") || !strcmp(kind, "OverflowError")
        || !strcmp(kind, "ZeroDivisionError")) return "ArithmeticError";
    if (!strcmp(kind, "InterruptedError") || !strcmp(kind, "BlockingIOError")
        || !strcmp(kind, "ChildProcessError") || !strcmp(kind, "FileExistsError")
        || !strcmp(kind, "FileNotFoundError") || !strcmp(kind, "IsADirectoryError")
        || !strcmp(kind, "NotADirectoryError") || !strcmp(kind, "PermissionError")
        || !strcmp(kind, "ProcessLookupError") || !strcmp(kind, "TimeoutError")
        || !strcmp(kind, "BrokenPipeError") || !strcmp(kind, "ConnectionAbortedError")
        || !strcmp(kind, "ConnectionRefusedError") || !strcmp(kind, "ConnectionResetError")
        || !strcmp(kind, "ConnectionError")) return "OSError";
    if (!strcmp(kind, "UnicodeDecodeError") || !strcmp(kind, "UnicodeEncodeError")
        || !strcmp(kind, "UnicodeTranslateError")) return "UnicodeError";
    if (!strcmp(kind, "IndentationError") || !strcmp(kind, "TabError")) return "SyntaxError";
    if (!strcmp(kind, "TabError")) return "IndentationError";
    /* registered config-write family (subclasses of Exception via RuntimeError) */
    if (!strcmp(kind, "ConfigWriteError") || !strcmp(kind, "SessionNotOwned"))
        return "RuntimeError";
    if (!strcmp(kind, "ManagedKeyError")) return "ConfigWriteError";
    return "Exception"; /* unknown scripted kinds default to the Exception family */
}

static bool tset_exc_is_exception(const char *kind) {
    if (!kind) return false;
    if (tset_is_base_exc(kind)) return false;
    const char *k = kind;
    for (int i = 0; i < 16 && k; i++) {
        if (!strcmp(k, "Exception")) return true;
        k = tset_exc_parent(k);
    }
    return false;
}

/* except (ConfigWriteError, SessionNotOwned) -- ManagedKeyError subclasses
 * ConfigWriteError, so the whole family passes through the guard. */
static bool tset_exc_is_config_write(const char *kind) {
    return kind && (!strcmp(kind, "ConfigWriteError") || !strcmp(kind, "ManagedKeyError")
                    || !strcmp(kind, "SessionNotOwned"));
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool tset_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static const char *tset_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static json_t *tset_err_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* Python type name for a JSON value (fixture scalar/container domain). */
static const char *tset_type_name(const json_t *v) {
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

static char *tset_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

/* json_string copies its input; this adopts+free an owned char* buffer. */
static json_t *tset_str_own(char *s) {
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

/* Python str() for scalars (containers are outside the str()-projection
 * fixture domain). */
static char *tset_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return tset_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return tset_strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return tset_strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && d > -9.2e18 && d < 9.2e18)
                snprintf(buf, sizeof buf, "%lld", (long long)d);
            else
                snprintf(buf, sizeof buf, "%.17g", d);
            return tset_strdup(buf);
        }
        default: return json_serialize(v);
    }
}

/* Python repr() for strings (ASCII corpus; quote choice minimises escapes). */
static char *tset_repr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return tset_strdup("None");
    if (v->type == JSON_BOOL) return tset_strdup(v->bool_val ? "True" : "False");
    if (v->type != JSON_STRING) return tset_pystr(v);
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
static bool tset_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f'
        || c == '\v' || (c >= 0x1c && c <= 0x1f);
}

static char *tset_strip(const char *s) {
    if (!s) s = "";
    while (*s && tset_is_ws((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && tset_is_ws((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - s);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

static char *tset_lower(const char *s) {
    char *o = tset_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}

/* str(value or "").strip().lower() -- _word. Falsy collapses to the EMPTY
 * STRING (value or ""), not str(None). */
static char *tset_word_v(const json_t *value) {
    char *s = tset_truthy(value) ? tset_pystr(value) : tset_strdup("");
    char *st = tset_strip(s);
    free(s);
    char *lo = tset_lower(st);
    free(st);
    return lo;
}

/* _raw_word: only None is blank; falsy non-strings keep their text. */
static char *tset_raw_word_v(const json_t *value) {
    char *s;
    if (!value || value->type == JSON_NULL) s = tset_strdup("");
    else s = tset_pystr(value);
    char *st = tset_strip(s);
    free(s);
    char *lo = tset_lower(st);
    free(st);
    return lo;
}

/* Deep scalar equality (Python == on the fixture domain; bool==int cross). */
static bool tset_json_eq(const json_t *a, const json_t *b) {
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
            if (!tset_json_eq(a->c.items[i], b->c.items[i])) return false;
        return true;
    }
    if (a->type == JSON_OBJECT) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !tset_json_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}

/* membership in a fixed string set (NULL-terminated array). */
static bool tset_in_set(const char *s, const char *const *set) {
    if (!s) return false;
    for (size_t i = 0; set[i]; i++) if (!strcmp(s, set[i])) return true;
    return false;
}

/* -- raise-producing access helpers -- */

/* d[key] -- borrowed value or the live KeyError/TypeError spec. */
static const json_t *tset_subscript(const json_t *obj, const char *key, json_t **raise) {
    *raise = NULL;
    if (obj && obj->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(obj, key);
        if (v) return v;
        char msg[256];
        snprintf(msg, sizeof msg, "'%s'", key); /* str(KeyError) == repr(key) */
        *raise = tset_err_spec("KeyError", msg);
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
        snprintf(msg, sizeof msg, "'%s' object is not subscriptable", tset_type_name(obj));
    *raise = tset_err_spec("TypeError", msg);
    return NULL;
}

/* Attribute access on a SimpleNamespace double: borrowed value (or borrowed
 * default when has_default and absent); missing required attribute raises the
 * live AttributeError. Non-dict targets raise the type-name message (or the
 * default when has_default -- getattr swallows). */
static const json_t *tset_attr(const json_t *obj, const char *key,
                               bool has_default, const json_t *def, json_t **raise) {
    *raise = NULL;
    if (obj && obj->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(obj, key);
        if (v) return v;
        if (has_default) return def;
        char msg[192];
        snprintf(msg, sizeof msg,
                 "'types.SimpleNamespace' object has no attribute '%s'", key);
        *raise = tset_err_spec("AttributeError", msg);
        return NULL;
    }
    if (has_default) return def;
    char msg[192];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'",
             tset_type_name(obj), key);
    *raise = tset_err_spec("AttributeError", msg);
    return NULL;
}

/* x.get(key[, default]) on a dict -- non-dict raises AttributeError 'get'.
 * Returns a borrowed value (or def). *raise set on a non-dict receiver. */
static const json_t *tset_dict_get(const json_t *obj, const char *key,
                                   const json_t *def, json_t **raise) {
    *raise = NULL;
    if (obj && obj->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(obj, key);
        return v ? v : def;
    }
    char msg[160];
    snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", tset_type_name(obj));
    *raise = tset_err_spec("AttributeError", msg);
    return NULL;
}

/* attribute WRITE on an agent double: dict -> set; non-dict -> AttributeError. */
static json_t *tset_attr_set(const json_t *obj, const char *key, json_t *val) {
    if (obj && obj->type == JSON_OBJECT) { json_set((json_t *)obj, key, val); return NULL; }
    char msg[192];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'",
             tset_type_name(obj), key);
    json_free(val);
    return tset_err_spec("AttributeError", msg);
}

/* a, b = v -- the live unpack messages (fixture domain: 2-lists, null/int/bool/
 * dict; str is iterable live but outside the unpack-script domain). */
static json_t *tset_unpack2(const json_t *v, json_t **a, json_t **b) {
    *a = *b = NULL;
    if (v && v->type == JSON_ARRAY) {
        if (v->c.count < 2) {
            char msg[96];
            snprintf(msg, sizeof msg, "not enough values to unpack (expected 2, got %zu)",
                     v->c.count);
            return tset_err_spec("ValueError", msg);
        }
        if (v->c.count > 2) {
            char msg[96];
            snprintf(msg, sizeof msg, "too many values to unpack (expected 2, got %zu)",
                     v->c.count);
            return tset_err_spec("ValueError", msg);
        }
        *a = json_copy(v->c.items[0]);
        *b = json_copy(v->c.items[1]);
        return NULL;
    }
    char msg[96];
    snprintf(msg, sizeof msg, "cannot unpack non-iterable %s object", tset_type_name(v));
    return tset_err_spec("TypeError", msg);
}

/* `with <non-CM>:` -- the py3.14 context-manager protocol TypeError. */
static json_t *tset_cm_error(const json_t *v) {
    char msg[192];
    snprintf(msg, sizeof msg,
             "'%s' object does not support the context manager protocol "
             "(missed __exit__ method)", tset_type_name(v));
    return tset_err_spec("TypeError", msg);
}

/* {**v} on a non-mapping. */
static json_t *tset_mapping_error(const json_t *v) {
    char msg[128];
    snprintf(msg, sizeof msg, "'%s' object is not a mapping", tset_type_name(v));
    return tset_err_spec("TypeError", msg);
}

static json_t *tset_iter_error(const json_t *v) {
    char msg[128];
    snprintf(msg, sizeof msg, "'%s' object is not iterable", tset_type_name(v));
    return tset_err_spec("TypeError", msg);
}

/* -- the one universal seam (fail-closed) -- */
static json_t *tset_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return tset_err_spec(type, msg);
}

static json_t *tset_call(tset_ctx_t *ctx, const char *name, json_t *args) {
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
        ret = tset_err_spec("RuntimeError", msg);
        ctx->last_raise = true;
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = tset_err_spec("RuntimeError", msg);
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
                ret = tset_make_exc(raise);
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
static json_t *tset_args0(void) { return json_object(); }
static void tset_aset(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static void tset_asets(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void tset_asetb(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void tset_asetn(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }

/* _err(rid, code, msg) / _ok(rid, result) -- server-global envelope seams. */
static json_t *tset_err(tset_ctx_t *ctx, const json_t *rid, int code, const char *msg) {
    json_t *a = tset_args0();
    tset_aset(a, "rid", rid);
    tset_asetn(a, "code", (double)code);
    tset_asets(a, "message", msg);
    return tset_call(ctx, "err", a);
}

static json_t *tset_ok(tset_ctx_t *ctx, const json_t *rid, json_t *result) {
    json_t *a = tset_args0();
    tset_aset(a, "rid", rid);
    json_set(a, "result", result ? result : json_null()); /* adopted */
    return tset_call(ctx, "ok", a);
}

/* session carrier accessor (borrowed; NULL when absent/null). The dispatcher
 * may override the session a handler sees (stale session_id -> NULL). */
static json_t *tset_session(tset_ctx_t *ctx) {
    if (ctx->has_override) return ctx->session_override;
    json_t *s = json_obj_get(ctx->state, "session");
    return (s && s->type == JSON_OBJECT) ? s : NULL;
}

/* -- pure leaves -- */

/* PoP: tset_word @ tui_gateway/methods_config_set.py:_word */
static json_t *tset_word(tset_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    char *w = tset_word_v(json_obj_get(args, "value"));
    json_t *r = json_string(w ? w : "");
    free(w);
    return r;
}

/* PoP: tset_raw_word @ tui_gateway/methods_config_set.py:_raw_word */
static json_t *tset_raw_word(tset_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    char *w = tset_raw_word_v(json_obj_get(args, "value"));
    json_t *r = json_string(w ? w : "");
    free(w);
    return r;
}

/* _kv(rid, key, value, **extra) -> _ok(rid, {"key","value",**extra}). */
static json_t *tset_kv(tset_ctx_t *ctx, const json_t *rid, const json_t *key,
                       const json_t *value, json_t *extra) {
    json_t *result = json_object();
    json_set(result, "key", key ? json_copy(key) : json_null());
    json_set(result, "value", value ? json_copy(value) : json_null());
    if (extra && extra->type == JSON_OBJECT) {
        for (size_t i = 0; i < extra->c.count; i++)
            json_set(result, extra->c.keys[i], json_copy(extra->c.items[i]));
    }
    json_free(extra);
    return tset_ok(ctx, rid, result);
}

/* PoP: tset_kv @ tui_gateway/methods_config_set.py:_kv */
static json_t *tset_kv_case(tset_ctx_t *ctx, const json_t *args) {
    json_t *extra = json_obj_get(args, "extra");
    return tset_kv(ctx, json_obj_get(args, "rid"), json_obj_get(args, "key"),
                   json_obj_get(args, "value"), extra ? json_copy(extra) : NULL);
}

/* _cfgset_model_ok(rid, key, value, warning="", confirm_message="",
 *                  scope="session", **extra). */
static json_t *tset_cfgset_model_ok(tset_ctx_t *ctx, const json_t *rid,
                                    const json_t *key, const json_t *value,
                                    const char *warning, const char *confirm_message,
                                    const char *scope, json_t *extra) {
    json_t *ex = json_object();
    json_set_hdr(ex, "warning", warning ? warning : "");
    json_set(ex, "confirm_required", json_bool(confirm_message && confirm_message[0]));
    json_set_hdr(ex, "confirm_message", confirm_message ? confirm_message : "");
    json_set_hdr(ex, "scope", scope ? scope : "session");
    if (extra && extra->type == JSON_OBJECT) {
        for (size_t i = 0; i < extra->c.count; i++)
            json_set(ex, extra->c.keys[i], json_copy(extra->c.items[i]));
    }
    json_free(extra);
    return tset_kv(ctx, rid, key, value, ex);
}

/* PoP: tset_cfgset_model_ok @ tui_gateway/methods_config_set.py:_cfgset_model_ok */
static json_t *tset_cfgset_model_ok_case(tset_ctx_t *ctx, const json_t *args) {
    json_t *extra = json_obj_get(args, "extra");
    return tset_cfgset_model_ok(ctx, json_obj_get(args, "rid"), json_obj_get(args, "key"),
                                json_obj_get(args, "value"),
                                json_get_str(args, "warning", ""),
                                json_get_str(args, "confirm_message", ""),
                                json_get_str(args, "scope", "session"),
                                extra ? json_copy(extra) : NULL);
}

/* _emit_session_info(sid, session): agent=session.get("agent"); if not None:
 * _emit("session.info", sid, _session_info(agent, session)). */
static json_t *tset_emit_session_info(tset_ctx_t *ctx, const json_t *sid, json_t *session) {
    json_t *raise = NULL;
    const json_t *agent = tset_dict_get(session, "agent", NULL, &raise);
    if (raise) return raise;
    if (!agent || agent->type == JSON_NULL) return NULL;
    json_t *a = tset_args0();
    tset_aset(a, "agent", agent);
    tset_aset(a, "session", session);
    json_t *info = tset_call(ctx, "session_info", a);
    if (tset_is_raise(info)) return info;
    json_t *ea = tset_args0();
    tset_asets(ea, "event", "session.info");
    char *sids = sid ? tset_pystr(sid) : tset_strdup("");
    tset_asets(ea, "sid", sids);
    free(sids);
    json_set(ea, "payload", info); /* adopted */
    json_t *er = tset_call(ctx, "emit", ea);
    if (tset_is_raise(er)) return er;
    json_free(er);
    return NULL;
}

/* PoP: tset_emit_session_info @ tui_gateway/methods_config_set.py:_emit_session_info */
static json_t *tset_emit_session_info_case(tset_ctx_t *ctx, const json_t *args) {
    return tset_emit_session_info(ctx, json_obj_get(args, "sid"), tset_session(ctx));
}

/* _emit_all_session_info(): for sid, sess in list(_sessions.items()):
 * _emit_session_info(sid, sess). */
static json_t *tset_emit_all_session_info(tset_ctx_t *ctx) {
    json_t *items = tset_call(ctx, "sessions_items", tset_args0());
    if (tset_is_raise(items)) return items;
    if (items && items->type != JSON_ARRAY) {
        json_t *ie = tset_iter_error(items);
        json_free(items);
        return ie;
    }
    json_t *ret = NULL;
    if (items) {
        for (size_t i = 0; i < json_len(items); i++) {
            const json_t *pair = json_get(items, i);
            json_t *sid = NULL, *sess = NULL;
            json_t *ur = tset_unpack2(pair, &sid, &sess);
            if (ur) { ret = ur; break; }
            json_t *r = tset_emit_session_info(ctx, sid, sess);
            json_free(sid); json_free(sess);
            if (r) { ret = r; break; }
        }
    }
    json_free(items);
    return ret;
}

/* PoP: tset_emit_all_session_info @ tui_gateway/methods_config_set.py:_emit_all_session_info */
static json_t *tset_emit_all_session_info_case(tset_ctx_t *ctx, const json_t *args) {
    (void)args;
    return tset_emit_all_session_info(ctx);
}

/* _cfgset_await_agent(session, rid): init_err=_wait_agent(session, rid);
 * if init_err: return it; return _err(5032) if session.get("agent") is None
 * else None. */
static json_t *tset_cfgset_await_agent(tset_ctx_t *ctx, json_t *session, const json_t *rid) {
    json_t *a = tset_args0();
    tset_aset(a, "session", session);
    tset_aset(a, "rid", rid);
    json_t *init_err = tset_call(ctx, "wait_agent", a);
    if (tset_is_raise(init_err)) return init_err;
    if (tset_truthy(init_err)) return init_err;
    json_free(init_err);
    json_t *raise = NULL;
    const json_t *agent = tset_dict_get(session, "agent", NULL, &raise);
    if (raise) return raise;
    if (!agent || agent->type == JSON_NULL)
        return tset_err(ctx, rid, 5032, "agent initialization failed");
    return NULL;
}

/* PoP: tset_cfgset_await_agent @ tui_gateway/methods_config_set.py:_cfgset_await_agent */
static json_t *tset_cfgset_await_agent_case(tset_ctx_t *ctx, const json_t *args) {
    return tset_cfgset_await_agent(ctx, tset_session(ctx), json_obj_get(args, "rid"));
}

/* -- managed-key refusal -- */

/* _refuse_managed_keys(*key_paths): first managed key raises ManagedKeyError. */
static json_t *tset_refuse_managed_keys(tset_ctx_t *ctx, const json_t *key_paths) {
    if (key_paths && key_paths->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(key_paths); i++) {
            const json_t *kp = json_get(key_paths, i);
            char *kps = tset_pystr(kp);
            json_t *a = tset_args0();
            tset_asets(a, "key_path", kps);
            json_t *managed = tset_call(ctx, "is_key_managed", a);
            if (tset_is_raise(managed)) { free(kps); return managed; }
            bool m = tset_truthy(managed);
            json_free(managed);
            if (m) {
                json_t *sa = tset_args0();
                tset_asets(sa, "path", "config.yaml");
                json_t *src = tset_call(ctx, "managed_source", sa);
                if (tset_is_raise(src)) { free(kps); return src; }
                char *srcs = tset_pystr(src);
                json_free(src);
                size_t n = strlen(kps) + strlen(srcs) + 160;
                char *msg = malloc(n);
                if (msg) snprintf(msg, n,
                    "Cannot set '%s': it is managed by your administrator (%s) "
                    "and cannot be changed. Contact your administrator to modify it.",
                    kps, srcs);
                free(srcs);
                json_t *e = tset_err_spec("ManagedKeyError", msg ? msg : "");
                free(msg);
                free(kps);
                return e;
            }
            free(kps);
        }
    }
    return NULL;
}

/* PoP: tset_refuse_managed_keys @ tui_gateway/methods_config_set.py:_refuse_managed_keys */
static json_t *tset_refuse_managed_keys_case(tset_ctx_t *ctx, const json_t *args) {
    return tset_refuse_managed_keys(ctx, json_obj_get(args, "key_paths"));
}

/* _refuse_managed_model_pick(reasoning_effort="", result=None). */
static json_t *tset_refuse_managed_model_pick(tset_ctx_t *ctx, const json_t *reasoning_effort,
                                              const json_t *result) {
    json_t *mk = tset_call(ctx, "managed_config_keys", tset_args0());
    if (tset_is_raise(mk)) return mk;
    bool any = tset_truthy(mk);
    json_free(mk);
    if (!any) return NULL;
    static const char *default_plan[] = {"default", "provider", "base_url", "api_mode", NULL};
    json_t *plan = json_array();
    if (result && result->type != JSON_NULL) {
        json_t *raw = tset_call(ctx, "read_user_config_raw", tset_args0());
        if (tset_is_raise(raw)) { json_free(plan); return raw; }
        json_t *gr = NULL;
        const json_t *model = tset_dict_get(raw, "model", NULL, &gr);
        if (gr) { json_free(raw); json_free(plan); return gr; }
        json_t *a = tset_args0();
        tset_aset(a, "result", result);
        tset_aset(a, "model", model);
        json_t *updates = tset_call(ctx, "model_selection_config_updates", a);
        json_free(raw);
        if (tset_is_raise(updates)) { json_free(plan); return updates; }
        if (!updates || updates->type != JSON_ARRAY) {
            json_t *ie = tset_iter_error(updates);
            json_free(updates); json_free(plan);
            return ie;
        }
        for (size_t i = 0; i < json_len(updates); i++)
            json_append(plan, json_copy(json_get(updates, i)));
        json_free(updates);
    } else {
        for (size_t i = 0; default_plan[i]; i++) json_append(plan, json_string(default_plan[i]));
    }
    /* effort_key = ("agent.reasoning_effort",) if parse_reasoning_effort(effort or None) */
    json_t *pa = tset_args0();
    tset_aset(pa, "raw", tset_truthy(reasoning_effort) ? reasoning_effort : NULL);
    json_t *parsed = tset_call(ctx, "parse_reasoning_effort", pa);
    if (tset_is_raise(parsed)) { json_free(plan); return parsed; }
    bool effort = tset_truthy(parsed);
    json_free(parsed);
    json_t *key_paths = json_array();
    for (size_t i = 0; i < json_len(plan); i++) {
        char *ks = tset_pystr(json_get(plan, i));
        size_t n = strlen(ks) + 8;
        char *buf = malloc(n);
        if (buf) snprintf(buf, n, "model.%s", ks);
        json_append(key_paths, json_string(buf ? buf : ""));
        free(buf); free(ks);
    }
    json_free(plan);
    if (effort) json_append(key_paths, json_string("agent.reasoning_effort"));
    json_t *r = tset_refuse_managed_keys(ctx, key_paths);
    json_free(key_paths);
    return r;
}

/* PoP: tset_refuse_managed_model_pick @ tui_gateway/methods_config_set.py:_refuse_managed_model_pick */
static json_t *tset_refuse_managed_model_pick_case(tset_ctx_t *ctx, const json_t *args) {
    return tset_refuse_managed_model_pick(ctx, json_obj_get(args, "reasoning_effort"),
                                          json_obj_get(args, "result"));
}

/* _write_display_sections(*, sections=None, drop_sections=(), **display_fields). */
static json_t *tset_write_display_sections(tset_ctx_t *ctx, const json_t *sections,
                                           const json_t *drop_sections,
                                           const json_t *display_fields) {
    /* key_paths: display.<field> for display_fields, then display.sections.<name>
     * for (*(sections or {}), *drop_sections). */
    json_t *key_paths = json_array();
    if (display_fields && display_fields->type == JSON_OBJECT) {
        for (size_t i = 0; i < display_fields->c.count; i++) {
            size_t n = strlen(display_fields->c.keys[i]) + 16;
            char *buf = malloc(n);
            if (buf) snprintf(buf, n, "display.%s", display_fields->c.keys[i]);
            json_append(key_paths, json_string(buf ? buf : ""));
            free(buf);
        }
    }
    const json_t *sec = tset_truthy(sections) ? sections : NULL;
    if (sec && sec->type == JSON_OBJECT) {
        for (size_t i = 0; i < sec->c.count; i++) {
            size_t n = strlen(sec->c.keys[i]) + 24;
            char *buf = malloc(n);
            if (buf) snprintf(buf, n, "display.sections.%s", sec->c.keys[i]);
            json_append(key_paths, json_string(buf ? buf : ""));
            free(buf);
        }
    }
    if (drop_sections && drop_sections->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(drop_sections); i++) {
            char *ns = tset_pystr(json_get(drop_sections, i));
            size_t n = strlen(ns) + 24;
            char *buf = malloc(n);
            if (buf) snprintf(buf, n, "display.sections.%s", ns);
            json_append(key_paths, json_string(buf ? buf : ""));
            free(buf); free(ns);
        }
    }
    json_t *rr = tset_refuse_managed_keys(ctx, key_paths);
    json_free(key_paths);
    if (rr) return rr;
    json_t *sft = tset_call(ctx, "supports_file_tooling", tset_args0());
    if (tset_is_raise(sft)) return sft;
    bool file_tooling = tset_truthy(sft);
    json_free(sft);
    if (!file_tooling) {
        json_t *sets = json_object();
        if (display_fields && display_fields->type == JSON_OBJECT) {
            for (size_t i = 0; i < display_fields->c.count; i++) {
                size_t n = strlen(display_fields->c.keys[i]) + 16;
                char *buf = malloc(n);
                if (buf) snprintf(buf, n, "display.%s", display_fields->c.keys[i]);
                json_set(sets, buf ? buf : "", json_copy(display_fields->c.items[i]));
                free(buf);
            }
        }
        if (sec && sec->type == JSON_OBJECT) {
            for (size_t i = 0; i < sec->c.count; i++) {
                size_t n = strlen(sec->c.keys[i]) + 24;
                char *buf = malloc(n);
                if (buf) snprintf(buf, n, "display.sections.%s", sec->c.keys[i]);
                json_set(sets, buf ? buf : "", json_copy(sec->c.items[i]));
                free(buf);
            }
        }
        json_t *drops = json_array();
        if (drop_sections && drop_sections->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(drop_sections); i++) {
                char *ns = tset_pystr(json_get(drop_sections, i));
                size_t n = strlen(ns) + 24;
                char *buf = malloc(n);
                if (buf) snprintf(buf, n, "display.sections.%s", ns);
                json_append(drops, json_string(buf ? buf : ""));
                free(buf); free(ns);
            }
        }
        json_t *a = tset_args0();
        json_set(a, "sets", sets);
        json_set(a, "drops", drops);
        json_t *wr = tset_call(ctx, "write_config_changes", a);
        if (tset_is_raise(wr)) return wr;
        json_free(wr);
        return NULL;
    }
    json_t *cfg = tset_call(ctx, "load_cfg_raw", tset_args0());
    if (tset_is_raise(cfg)) return cfg;
    if (!cfg || cfg->type != JSON_OBJECT) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", tset_type_name(cfg));
        json_free(cfg);
        return tset_err_spec("AttributeError", msg);
    }
    const json_t *disp0 = json_obj_get(cfg, "display");
    json_t *display = (disp0 && disp0->type == JSON_OBJECT) ? json_copy(disp0) : json_object();
    const json_t *cur0 = json_obj_get(display, "sections");
    json_t *cur = (cur0 && cur0->type == JSON_OBJECT) ? json_copy(cur0) : json_object();
    if (display_fields && display_fields->type == JSON_OBJECT)
        for (size_t i = 0; i < display_fields->c.count; i++)
            json_set(display, display_fields->c.keys[i], json_copy(display_fields->c.items[i]));
    if (sec && sec->type == JSON_OBJECT)
        for (size_t i = 0; i < sec->c.count; i++)
            json_set(cur, sec->c.keys[i], json_copy(sec->c.items[i]));
    if (drop_sections && drop_sections->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(drop_sections); i++) {
            char *ns = tset_pystr(json_get(drop_sections, i));
            json_obj_del(cur, ns);
            free(ns);
        }
    json_set(display, "sections", cur);
    json_set(cfg, "display", display);
    json_t *a = tset_args0();
    json_set(a, "cfg", json_copy(cfg));
    json_t *sv = tset_call(ctx, "save_cfg", a);
    json_free(cfg);
    if (tset_is_raise(sv)) return sv;
    json_free(sv);
    return NULL;
}

/* PoP: tset_write_display_sections @ tui_gateway/methods_config_set.py:_write_display_sections */
static json_t *tset_write_display_sections_case(tset_ctx_t *ctx, const json_t *args) {
    return tset_write_display_sections(ctx, json_obj_get(args, "sections"),
                                       json_obj_get(args, "drop_sections"),
                                       json_obj_get(args, "display_fields"));
}

/* -- the guard wrapper (ConfigWriteError/SessionNotOwned pass; Exception -> 5001) -- */
static json_t *tset_guard_catch(tset_ctx_t *ctx, const json_t *rid, json_t *ret) {
    if (!tset_is_raise(ret)) return ret;
    const char *kind = tset_raise_kind(ret);
    if (tset_exc_is_config_write(kind) || tset_is_base_exc(kind)) return ret;
    char *msg = tset_strdup(json_get_str(ret, "message", ""));
    json_free(ret);
    json_t *e = tset_err(ctx, rid, 5001, msg);
    free(msg);
    return e;
}

/* PoP: tset_cfgset_guarded @ tui_gateway/methods_config_set.py:_cfgset_guarded */
static json_t *tset_cfgset_guarded_case(tset_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    json_t *a = tset_args0();
    tset_aset(a, "rid", rid);
    tset_aset(a, "params", json_obj_get(args, "params"));
    tset_aset(a, "key", json_obj_get(args, "key"));
    tset_aset(a, "value", json_obj_get(args, "value"));
    tset_aset(a, "session", tset_session(ctx));
    json_t *ret = tset_call(ctx, "guarded_fn", a);
    return tset_guard_catch(ctx, rid, ret);
}

/* -- model-switch stash -- */

/* parsed double attribute read with a getattr default (borrowed). */
static const json_t *tset_parsed_get(const json_t *parsed, const char *key,
                                     const json_t *def, json_t **raise) {
    return tset_attr(parsed, key, true, def, raise);
}

static json_t *tset_stash_pending_model_switch(tset_ctx_t *ctx, const json_t *rid,
                                               const json_t *key, const json_t *value,
                                               json_t *session, bool confirmed,
                                               const json_t *parsed) {
    json_t *raise = NULL;
    json_t *false_node = json_bool(false);
    const json_t *is_reset_v = tset_parsed_get(parsed, "is_reset", false_node, &raise);
    bool is_reset = tset_truthy(is_reset_v);
    if (raise) { json_free(false_node); return raise; }
    if (!is_reset) {
        const json_t *ig = tset_attr(parsed, "is_global", false, NULL, &raise);
        if (raise) { json_free(false_node); return raise; }
        const json_t *is = tset_attr(parsed, "is_session", false, NULL, &raise);
        if (raise) { json_free(false_node); return raise; }
        const json_t *io = tset_attr(parsed, "is_once", false, NULL, &raise);
        if (raise) { json_free(false_node); return raise; }
        const json_t *ep = tset_attr(parsed, "explicit_provider", false, NULL, &raise);
        if (raise) { json_free(false_node); return raise; }
        json_t *a = tset_args0();
        tset_aset(a, "is_global", ig);
        tset_aset(a, "is_session", is);
        tset_aset(a, "is_once", io);
        tset_aset(a, "explicit_provider", ep);
        json_t *persist = tset_call(ctx, "resolve_persist_behavior", a);
        if (tset_is_raise(persist)) { json_free(false_node); return persist; }
        bool do_persist = tset_truthy(persist);
        json_free(persist);
        if (do_persist) {
            const json_t *re = tset_parsed_get(parsed, "reasoning_effort", NULL, &raise);
            if (raise) { json_free(false_node); return raise; }
            json_t *rr = tset_refuse_managed_model_pick(ctx, re, NULL);
            if (rr) { json_free(false_node); return rr; }
        }
    }
    json_free(false_node);
    /* try: pending_model = parsed.model_input except Exception: str(value) */
    json_t *pending_model = NULL;
    const json_t *mi_raise = json_obj_get(parsed, "model_input_raise");
    if (mi_raise && mi_raise->type == JSON_OBJECT) {
        json_t *exc = tset_make_exc(mi_raise);
        const char *kind = tset_raise_kind(exc);
        if (tset_is_base_exc(kind)) return exc;
        json_free(exc);
        pending_model = tset_str_own(tset_pystr(value));
    } else {
        const json_t *mi = json_obj_get(parsed, "model_input");
        pending_model = mi ? json_copy(mi) : json_null();
    }
    /* pending_provider = (getattr(parsed,"explicit_provider","") or "").strip() */
    const json_t *ep2 = tset_parsed_get(parsed, "explicit_provider", NULL, &raise);
    if (raise) { json_free(pending_model); return raise; }
    char *eps = tset_truthy(ep2) ? tset_pystr(ep2) : tset_strdup("");
    char *pending_provider = tset_strip(eps);
    free(eps);
    if (is_reset) {
        json_t *tgt = tset_call(ctx, "config_model_target", tset_args0());
        if (tset_is_raise(tgt)) { json_free(pending_model); free(pending_provider); return tgt; }
        json_t *tm = NULL, *tp = NULL;
        json_t *ur = tset_unpack2(tgt, &tm, &tp);
        json_free(tgt);
        if (ur) { json_free(pending_model); free(pending_provider); return ur; }
        json_free(pending_model);
        pending_model = tset_truthy(tm) ? tm : tset_str_own(tset_pystr(value));
        if (!tset_truthy(tm)) json_free(tm);
        free(pending_provider);
        pending_provider = tset_pystr(tp);
        json_free(tp);
    }
    if (!confirmed && !is_reset) {
        const json_t *agent = tset_dict_get(session, "agent", NULL, &raise);
        if (raise) { json_free(pending_model); free(pending_provider); return raise; }
        const json_t *ap = tset_attr(agent, "provider", true, NULL, &raise);
        if (raise) { json_free(pending_model); free(pending_provider); return raise; }
        char *aps = tset_truthy(ap) ? tset_pystr(ap) : tset_strdup("");
        char *apst = tset_strip(aps);
        free(aps);
        const char *guard_provider = pending_provider[0] ? pending_provider : apst;
        json_t *a = tset_args0();
        tset_aset(a, "model", pending_model);
        tset_asets(a, "provider", guard_provider);
        tset_aset(a, "agent", agent);
        json_t *warn = tset_call(ctx, "pending_switch_selection_warning", a);
        free(apst);
        if (tset_is_raise(warn)) { json_free(pending_model); free(pending_provider); return warn; }
        if (warn && warn->type != JSON_NULL) {
            char *ws = tset_pystr(warn);
            json_t *ex = json_object();
            json_set(ex, "deferred", json_bool(false));
            json_t *r = tset_cfgset_model_ok(ctx, rid, key, pending_model, ws, ws, "session", ex);
            free(ws);
            json_free(warn);
            json_free(pending_model);
            free(pending_provider);
            return r;
        }
        json_free(warn);
    }
    /* session["pending_model_switch"] = {...} */
    json_t *pms = json_object();
    json_set(pms, "raw", value ? json_copy(value) : json_null());
    json_set(pms, "confirm_expensive_model", json_bool(confirmed));
    json_set(pms, "display_model", json_copy(pending_model));
    json_set(pms, "display_provider", json_string(pending_provider));
    if (session && session->type == JSON_OBJECT) json_set(session, "pending_model_switch", pms);
    else json_free(pms);
    json_t *ex = json_object();
    json_set(ex, "deferred", json_bool(true));
    json_t *r = tset_cfgset_model_ok(ctx, rid, key, pending_model, "", "", "session", ex);
    json_free(pending_model);
    free(pending_provider);
    return r;
}

/* PoP: tset_stash_pending_model_switch @ tui_gateway/methods_config_set.py:_stash_pending_model_switch */
static json_t *tset_stash_pending_model_switch_case(tset_ctx_t *ctx, const json_t *args) {
    return tset_stash_pending_model_switch(ctx, json_obj_get(args, "rid"),
                                           json_obj_get(args, "key"),
                                           json_obj_get(args, "value"),
                                           tset_session(ctx),
                                           json_get_bool(args, "confirmed", false),
                                           json_obj_get(args, "parsed"));
}

/* -- shared constant tables -- */
static const char *const TSET_BOOL_TRUE[] = {"1", "on", "true", "yes", NULL};
static const char *const TSET_BOOL_FALSE[] = {"0", "off", "false", "no", NULL};

/* _BOOL_WORDS.get(word) -> fresh bool node or NULL (absent). */
static json_t *tset_bool_word(const char *w) {
    if (!w) return NULL;
    if (tset_in_set(w, TSET_BOOL_TRUE)) return json_bool(true);
    if (tset_in_set(w, TSET_BOOL_FALSE)) return json_bool(false);
    return NULL;
}

static const char *const TSET_DETAIL_SECTIONS[] = {"thinking", "tools", "subagents", "activity", NULL};
static const char *const TSET_DETAIL_MODES[] = {"hidden", "collapsed", "expanded", NULL};
static const char *const TSET_APPROVAL_MODES[] = {"manual", "smart", "off", NULL};
static const char *const TSET_STATUSBAR_MODES[] = {"off", "top", "bottom", NULL};
static const char *const TSET_INDICATOR_STYLES[] = {"ascii", "emoji", "kaomoji", "unicode", NULL};
static const char *const TSET_DISPLAY_TOGGLE_KEYS[] = {
    "display.message_reactions", "display.in_app_tips", "display.in_app_tours", NULL};
static const char *const TSET_STATIC_TIERS[] = {"priority", "ultrafast", NULL};
static const char *const TSET_SESSION_SCOPED_KEYS[] = {"model", "fast", "yolo", "reasoning", NULL};

#define TSET_AGENT_STILL_STARTING \
    "Hermes is still starting this session (loading tools), so this command could not run yet. " \
    "Wait for the status bar to show ready and try again."

/* _MOUSE_TRACKING_ALIASES.get(word) -> fresh string node or NULL. */
static json_t *tset_mouse_alias(const char *w) {
    if (!w) return NULL;
    static const char *const keys[] = {"0", "1", "all", "any", "button", "buttons", "click",
        "false", "full", "no", "off", "on", "scroll", "true", "wheel", "yes", NULL};
    static const char *const vals[] = {"off", "all", "all", "all", "buttons", "buttons",
        "buttons", "off", "all", "off", "off", "all", "wheel", "all", "wheel", "all", NULL};
    for (size_t i = 0; keys[i]; i++) if (!strcmp(w, keys[i])) return json_string(vals[i]);
    return NULL;
}

/* write_config_key(key, value) seam; returns raise spec or NULL (frees ok ret). */
static json_t *tset_write_config_key(tset_ctx_t *ctx, const char *key, const json_t *value) {
    json_t *a = tset_args0();
    tset_asets(a, "key", key);
    tset_aset(a, "value", value);
    json_t *r = tset_call(ctx, "write_config_key", a);
    if (tset_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* -- _set_word (inline _word_setters table) -- */
static json_t *tset_set_word_apply(tset_ctx_t *ctx, const char *key, const char *w) {
    if (!strcmp(key, "busy"))
        return tset_write_config_key(ctx, "display.busy_input_mode", json_string(w));
    if (!strcmp(key, "approvals.mode")) {
        json_t *r = tset_write_config_key(ctx, "approvals.mode", json_string(w));
        if (r) return r;
        return tset_emit_all_session_info(ctx);
    }
    if (!strcmp(key, "details_mode")) {
        json_t *sections = json_object();
        for (size_t i = 0; TSET_DETAIL_SECTIONS[i]; i++)
            json_set(sections, TSET_DETAIL_SECTIONS[i], json_string(w));
        json_t *fields = json_object();
        json_set(fields, "details_mode", json_string(w));
        json_t *r = tset_write_display_sections(ctx, sections, NULL, fields);
        json_free(sections);
        json_free(fields);
        return r;
    }
    if (!strcmp(key, "thinking_mode")) {
        json_t *updates = json_object();
        json_set(updates, "display.thinking_mode", json_string(w));
        json_set(updates, "display.details_mode",
                 json_string(!strcmp(w, "full") ? "expanded" : "collapsed"));
        json_t *a = tset_args0();
        json_set(a, "updates", updates);
        json_t *r = tset_call(ctx, "write_config_keys", a);
        if (tset_is_raise(r)) return r;
        json_free(r);
        return NULL;
    }
    if (!strcmp(key, "theme"))
        return tset_write_config_key(ctx, "display.tui_theme", json_string(w));
    if (!strcmp(key, "indicator"))
        return tset_write_config_key(ctx, "display.tui_status_indicator", json_string(w));
    if (!strcmp(key, "voice.voice_chat_mode"))
        return tset_write_config_key(ctx, "voice.voice_chat_mode", json_string(w));
    return NULL;
}

static json_t *tset_set_word(tset_ctx_t *ctx, const json_t *rid, const json_t *params,
                             const json_t *key, const json_t *value, json_t *session) {
    (void)params; (void)session;
    const char *k = tset_str(key);
    if (!k) k = "";
    /* _word_setters()[key] -- KeyError on an unknown key. */
    static const char *const keys[] = {"busy", "approvals.mode", "details_mode",
        "thinking_mode", "theme", "indicator", "voice.voice_chat_mode", NULL};
    bool known = tset_in_set(k, keys);
    if (!known) {
        char msg[256];
        snprintf(msg, sizeof msg, "'%s'", k);
        return tset_err_spec("KeyError", msg);
    }
    bool raw_word = !strcmp(k, "indicator");
    char *raw = raw_word ? tset_raw_word_v(value) : tset_word_v(value);
    const char *const *allowed = NULL;
    if (!strcmp(k, "busy")) { static const char *const a[] = {"queue", "steer", "interrupt", NULL}; allowed = a; }
    else if (!strcmp(k, "approvals.mode")) allowed = TSET_APPROVAL_MODES;
    else if (!strcmp(k, "details_mode")) allowed = TSET_DETAIL_MODES;
    else if (!strcmp(k, "thinking_mode")) { static const char *const a[] = {"collapsed", "truncated", "full", NULL}; allowed = a; }
    else if (!strcmp(k, "theme")) { static const char *const a[] = {"auto", "light", "dark", NULL}; allowed = a; }
    else if (!strcmp(k, "indicator")) allowed = TSET_INDICATOR_STYLES;
    else { static const char *const a[] = {"chained", "gpt-live", NULL}; allowed = a; }
    if (!tset_in_set(raw, allowed)) {
        char *vs = tset_pystr(value);
        char msg[512];
        if (!strcmp(k, "busy")) snprintf(msg, sizeof msg, "unknown busy mode: %s", vs);
        else if (!strcmp(k, "approvals.mode"))
            snprintf(msg, sizeof msg, "unknown approval mode: %s; pick one of manual|smart|off", vs);
        else if (!strcmp(k, "details_mode")) snprintf(msg, sizeof msg, "unknown details_mode: %s", vs);
        else if (!strcmp(k, "thinking_mode")) snprintf(msg, sizeof msg, "unknown thinking_mode: %s", vs);
        else if (!strcmp(k, "theme"))
            snprintf(msg, sizeof msg, "unknown theme value: %s (use auto|light|dark)", vs);
        else if (!strcmp(k, "indicator")) {
            json_t *raw_node = json_string(raw);
            char *rrepr = tset_repr(raw_node);
            json_free(raw_node);
            snprintf(msg, sizeof msg, "unknown indicator: %s; pick one of ascii|emoji|kaomoji|unicode", rrepr);
            free(rrepr);
        } else snprintf(msg, sizeof msg, "unknown voice chat mode: %s; pick chained|gpt-live", vs);
        free(vs);
        free(raw);
        return tset_err(ctx, rid, 4002, msg);
    }
    json_t *ar = tset_set_word_apply(ctx, k, raw);
    if (ar) { free(raw); return ar; }
    json_t *r = tset_kv(ctx, rid, key, json_string(raw), NULL);
    free(raw);
    return r;
}

/* PoP: tset_set_word @ tui_gateway/methods_config_set.py:_set_word */
static json_t *tset_set_word_case(tset_ctx_t *ctx, const json_t *args) {
    return tset_set_word(ctx, json_obj_get(args, "rid"), json_obj_get(args, "params"),
                         json_obj_get(args, "key"), json_obj_get(args, "value"),
                         tset_session(ctx));
}

/* -- _set_toggle (inline _toggle_setters table) -- */
static json_t *tset_toggle_flipped(tset_ctx_t *ctx, const char *key) {
    json_t *dc = tset_call(ctx, "display_cfg", tset_args0());
    if (tset_is_raise(dc)) return dc;
    json_t *raise = NULL;
    if (!strcmp(key, "statusbar")) {
        json_t *top = json_string("top");
        const json_t *v = tset_dict_get(dc, "tui_statusbar", top, &raise);
        if (raise) { json_free(top); json_free(dc); return raise; }
        json_t *a = tset_args0();
        tset_aset(a, "raw", v);
        json_free(top);
        json_t *co = tset_call(ctx, "coerce_statusbar", a);
        json_free(dc);
        if (tset_is_raise(co)) return co;
        const char *cs = tset_str(co);
        json_t *r = json_string(cs && !strcmp(cs, "off") ? "top" : "off");
        json_free(co);
        return r;
    }
    if (!strcmp(key, "mouse")) {
        json_t *a = tset_args0();
        tset_aset(a, "display", dc);
        json_t *mt = tset_call(ctx, "display_mouse_tracking", a);
        json_free(dc);
        if (tset_is_raise(mt)) return mt;
        const char *ms = tset_str(mt);
        json_t *r = json_string(ms && !strcmp(ms, "off") ? "all" : "off");
        json_free(mt);
        return r;
    }
    const char *field = !strcmp(key, "density") ? "tui_compact"
                      : !strcmp(key, "battery") ? "battery" : "timestamps";
    const json_t *v = tset_dict_get(dc, field, NULL, &raise);
    json_free(dc);
    if (raise) return raise;
    return json_bool(!tset_truthy(v));
}

static json_t *tset_set_toggle(tset_ctx_t *ctx, const json_t *rid, const json_t *params,
                               const json_t *key, const json_t *value, json_t *session) {
    (void)params; (void)session;
    const char *k = tset_str(key);
    if (!k) k = "";
    static const char *const keys[] = {"density", "battery", "timestamps", "statusbar", "mouse", NULL};
    if (!tset_in_set(k, keys)) {
        char msg[256];
        snprintf(msg, sizeof msg, "'%s'", k);
        return tset_err_spec("KeyError", msg);
    }
    bool raw_word = !strcmp(k, "mouse");
    char *raw = raw_word ? tset_raw_word_v(value) : tset_word_v(value);
    json_t *nv = NULL;
    bool owns_nv = true;
    if (!strcmp(raw, "") || !strcmp(raw, "toggle")) {
        nv = tset_toggle_flipped(ctx, k);
        if (tset_is_raise(nv)) { free(raw); return nv; }
    } else {
        if (!strcmp(k, "density")) {
            if (!strcmp(raw, "on")) nv = json_bool(true);
            else if (!strcmp(raw, "off")) nv = json_bool(false);
        } else if (!strcmp(k, "battery")) {
            /* battery aliases: on/true/yes -> True; off/false/no -> False (no 1/0) */
            static const char *const bt[] = {"on", "true", "yes", NULL};
            static const char *const bf[] = {"off", "false", "no", NULL};
            if (tset_in_set(raw, bt)) nv = json_bool(true);
            else if (tset_in_set(raw, bf)) nv = json_bool(false);
        } else if (!strcmp(k, "timestamps")) {
            nv = tset_bool_word(raw);
        } else if (!strcmp(k, "statusbar")) {
            if (!strcmp(raw, "on")) nv = json_string("top");
            else if (tset_in_set(raw, TSET_STATUSBAR_MODES)) nv = json_string(raw);
        } else { /* mouse */
            nv = tset_mouse_alias(raw);
        }
    }
    free(raw);
    if (!nv || nv->type == JSON_NULL) {
        if (nv) json_free(nv);
        char *vs = tset_pystr(value);
        size_t n = strlen(k) + strlen(vs) + 40;
        char *msg = malloc(n);
        if (msg) snprintf(msg, n, "unknown %s value: %s", k, vs);
        free(vs);
        json_t *e = tset_err(ctx, rid, 4002, msg ? msg : "");
        free(msg);
        return e;
    }
    const char *cfg_key = !strcmp(k, "density") ? "display.tui_compact"
                        : !strcmp(k, "battery") ? "display.battery"
                        : !strcmp(k, "timestamps") ? "display.timestamps"
                        : !strcmp(k, "statusbar") ? "display.tui_statusbar"
                        : "display.mouse_tracking";
    json_t *wr = tset_write_config_key(ctx, cfg_key, nv);
    if (wr) { json_free(nv); return wr; }
    /* report(nv): on_off for density/battery/timestamps; identity otherwise */
    json_t *reported;
    if (!strcmp(k, "density") || !strcmp(k, "battery") || !strcmp(k, "timestamps"))
        reported = json_string(tset_truthy(nv) ? "on" : "off");
    else
        reported = json_copy(nv);
    json_t *r = tset_kv(ctx, rid, key, reported, NULL);
    json_free(nv);
    (void)owns_nv;
    return r;
}

/* PoP: tset_set_toggle @ tui_gateway/methods_config_set.py:_set_toggle */
static json_t *tset_set_toggle_case(tset_ctx_t *ctx, const json_t *args) {
    return tset_set_toggle(ctx, json_obj_get(args, "rid"), json_obj_get(args, "params"),
                           json_obj_get(args, "key"), json_obj_get(args, "value"),
                           tset_session(ctx));
}

/* -- _set_busy / _set_approval_mode -- */

/* PoP: tset_set_busy @ tui_gateway/methods_config_set.py:_set_busy */
static json_t *tset_set_busy(tset_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *key = json_obj_get(args, "key");
    const json_t *value = json_obj_get(args, "value");
    char *w = tset_word_v(value);
    if (!strcmp(w, "") || !strcmp(w, "status")) {
        free(w);
        json_t *mode = tset_call(ctx, "load_busy_input_mode", tset_args0());
        if (tset_is_raise(mode)) return mode;
        return tset_kv(ctx, rid, key, mode, NULL);
    }
    free(w);
    return tset_set_word(ctx, rid, json_obj_get(args, "params"), key, value, tset_session(ctx));
}

/* PoP: tset_set_approval_mode @ tui_gateway/methods_config_set.py:_set_approval_mode */
static json_t *tset_set_approval_mode(tset_ctx_t *ctx, const json_t *args) {
    json_t *real_key = json_string("approvals.mode");
    json_t *r = tset_set_word(ctx, json_obj_get(args, "rid"), json_obj_get(args, "params"),
                              real_key, json_obj_get(args, "value"), tset_session(ctx));
    json_free(real_key);
    return r;
}

/* -- _set_details_section -- */

/* PoP: tset_set_details_section @ tui_gateway/methods_config_set.py:_set_details_section */
static json_t *tset_set_details_section(tset_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *key = json_obj_get(args, "key");
    const json_t *value = json_obj_get(args, "value");
    const char *k = tset_str(key);
    if (!k) k = "";
    /* section = key.split(".", 1)[1] -- IndexError when no ".". */
    const char *dot = strchr(k, '.');
    if (!dot) {
        return tset_err_spec("IndexError", "list index out of range");
    }
    const char *section = dot + 1;
    if (!tset_in_set(section, TSET_DETAIL_SECTIONS)) {
        char msg[256];
        snprintf(msg, sizeof msg, "unknown section: %s", section);
        return tset_err(ctx, rid, 4002, msg);
    }
    char *nv = tset_word_v(value);
    if (nv[0] && !tset_in_set(nv, TSET_DETAIL_MODES)) {
        char *vs = tset_pystr(value);
        size_t n = strlen(vs) + 40;
        char *msg = malloc(n);
        if (msg) snprintf(msg, n, "unknown details_mode: %s", vs);
        free(vs); free(nv);
        json_t *e = tset_err(ctx, rid, 4002, msg ? msg : "");
        free(msg);
        return e;
    }
    json_t *sections = NULL, *drops = NULL;
    if (nv[0]) {
        sections = json_object();
        json_set(sections, section, json_string(nv));
    } else {
        drops = json_array();
        json_append(drops, json_string(section));
    }
    json_t *r = tset_write_display_sections(ctx, sections, drops, NULL);
    json_free(sections);
    json_free(drops);
    if (r) { free(nv); return r; }
    json_t *kv = tset_kv(ctx, rid, key, json_string(nv), NULL);
    free(nv);
    return kv;
}

/* -- _set_display_toggle / _set_daybreak -- */
static json_t *tset_set_bool_key(tset_ctx_t *ctx, const json_t *args, const char *cfg_key) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *key = json_obj_get(args, "key");
    const json_t *value = json_obj_get(args, "value");
    char *vs = tset_pystr(value);
    char *st = tset_strip(vs);
    free(vs);
    char *lo = tset_lower(st);
    free(st);
    json_t *on = tset_bool_word(lo);
    free(lo);
    if (!on) {
        const char *k = tset_str(key);
        char msg[256];
        snprintf(msg, sizeof msg, "%s takes true or false", k ? k : "");
        return tset_err(ctx, rid, 4002, msg);
    }
    json_t *wr = tset_write_config_key(ctx, cfg_key, on);
    if (wr) { json_free(on); return wr; }
    return tset_kv(ctx, rid, key, on, NULL);
}

/* PoP: tset_set_display_toggle @ tui_gateway/methods_config_set.py:_set_display_toggle */
static json_t *tset_set_display_toggle(tset_ctx_t *ctx, const json_t *args) {
    const char *k = tset_str(json_obj_get(args, "key"));
    return tset_set_bool_key(ctx, args, k ? k : "");
}

/* PoP: tset_set_daybreak @ tui_gateway/methods_config_set.py:_set_daybreak */
static json_t *tset_set_daybreak(tset_ctx_t *ctx, const json_t *args) {
    return tset_set_bool_key(ctx, args, "agent.daybreak");
}

/* -- _set_skin -- */

/* PoP: tset_set_skin @ tui_gateway/methods_config_set.py:_set_skin */
static json_t *tset_set_skin_body(tset_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *key = json_obj_get(args, "key");
    const json_t *value = json_obj_get(args, "value");
    json_t *wr = tset_write_config_key(ctx, "display.skin", value);
    if (wr) return wr;
    json_t *skin = tset_call(ctx, "resolve_skin", tset_args0());
    if (tset_is_raise(skin)) return skin;
    json_t *a = tset_args0();
    tset_asets(a, "event", "skin.changed");
    json_set(a, "payload", skin); /* adopted */
    json_t *bc = tset_call(ctx, "broadcast_global_event", a);
    if (tset_is_raise(bc)) return bc;
    json_free(bc);
    json_t *note = tset_call(ctx, "note_skin_broadcast", tset_args0());
    if (tset_is_raise(note)) return note;
    json_free(note);
    return tset_kv(ctx, rid, key, value, NULL);
}

static json_t *tset_set_skin(tset_ctx_t *ctx, const json_t *args) {
    return tset_guard_catch(ctx, json_obj_get(args, "rid"), tset_set_skin_body(ctx, args));
}

/* -- _set_verbose -- */

/* PoP: tset_set_verbose @ tui_gateway/methods_config_set.py:_set_verbose */
static json_t *tset_set_verbose(tset_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *key = json_obj_get(args, "key");
    const json_t *value = json_obj_get(args, "value");
    json_t *session = tset_session(ctx);
    static const char *const cycle[] = {"off", "new", "all", "verbose", NULL};
    char *nv = NULL;
    bool vs_is_cycle = value && value->type == JSON_STRING
                       && value->str_val && !strcmp(value->str_val, "cycle");
    if (tset_truthy(value) && !vs_is_cycle) {
        char *vs = tset_pystr(value);
        char *st = tset_strip(vs);
        free(vs);
        nv = tset_lower(st);
        free(st);
        if (!tset_in_set(nv, cycle)) {
            char *vs2 = tset_pystr(value);
            size_t n = strlen(vs2) + 40;
            char *msg = malloc(n);
            if (msg) snprintf(msg, n, "unknown verbose mode: %s", vs2);
            free(vs2); free(nv);
            json_t *e = tset_err(ctx, rid, 4002, msg ? msg : "");
            free(msg);
            return e;
        }
    } else {
        /* cur = session.get("tool_progress_mode", _load_tool_progress_mode())
         * if session else _load_tool_progress_mode() -- the dict.get default is
         * EAGER: the seam fires even when the session carries the key. */
        json_t *mode = tset_call(ctx, "load_tool_progress_mode", tset_args0());
        if (tset_is_raise(mode)) return mode;
        const json_t *cur = mode;
        if (session) {
            json_t *gr = NULL;
            const json_t *v = tset_dict_get(session, "tool_progress_mode", mode, &gr);
            if (gr) { json_free(mode); return gr; }
            cur = v;
        }
        int idx = 2;
        const char *cs = tset_str(cur);
        if (cs) for (int i = 0; cycle[i]; i++) if (!strcmp(cs, cycle[i])) { idx = i; break; }
        nv = tset_strdup(cycle[(idx + 1) % 4]);
        json_free(mode);
    }
    json_t *wr = tset_write_config_key(ctx, "display.tool_progress", json_string(nv));
    if (wr) { free(nv); return wr; }
    if (session) {
        json_set(session, "tool_progress_mode", json_string(nv));
        json_t *gr = NULL;
        const json_t *agent = tset_dict_get(session, "agent", NULL, &gr);
        if (gr) { free(nv); return gr; }
        if (agent && agent->type != JSON_NULL) {
            json_t *ar = tset_attr_set(agent, "verbose_logging", json_bool(!strcmp(nv, "verbose")));
            if (ar) { free(nv); return ar; }
        }
    }
    json_t *r = tset_kv(ctx, rid, key, json_string(nv), NULL);
    free(nv);
    return r;
}

/* -- _set_focus -- */

/* PoP: tset_set_focus @ tui_gateway/methods_config_set.py:_set_focus */
static json_t *tset_set_focus(tset_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *key = json_obj_get(args, "key");
    const json_t *value = json_obj_get(args, "value");
    json_t *session = tset_session(ctx);
    json_t *dc = tset_call(ctx, "display_cfg", tset_args0());
    if (tset_is_raise(dc)) return dc;
    json_t *gr = NULL;
    const json_t *fv = tset_dict_get(dc, "focus_view", NULL, &gr);
    if (gr) { json_free(dc); return gr; }
    bool cur_focus = tset_truthy(fv);
    char *arg = tset_truthy(value) ? tset_pystr(value) : tset_strdup("");
    json_t *fa = tset_args0();
    tset_asets(fa, "arg", arg);
    tset_asetb(fa, "current", cur_focus);
    free(arg);
    json_t *rf = tset_call(ctx, "resolve_focus_arg", fa);
    if (tset_is_raise(rf)) { json_free(dc); return rf; }
    json_t *action = NULL, *target = NULL;
    json_t *ur = tset_unpack2(rf, &action, &target);
    json_free(rf);
    if (ur) { json_free(dc); return ur; }
    const char *as = tset_str(action);
    if (as && !strcmp(as, "usage")) {
        char *vs = tset_pystr(value);
        size_t n = strlen(vs) + 64;
        char *msg = malloc(n);
        if (msg) snprintf(msg, n, "unknown focus value: %s (use on|off|status)", vs);
        free(vs); json_free(action); json_free(target); json_free(dc);
        json_t *e = tset_err(ctx, rid, 4002, msg ? msg : "");
        free(msg);
        return e;
    }
    bool target_none = !target || target->type == JSON_NULL;
    if ((as && !strcmp(as, "status")) || target_none) {
        json_t *mode = tset_call(ctx, "load_tool_progress_mode", tset_args0());
        if (tset_is_raise(mode)) { json_free(action); json_free(target); json_free(dc); return mode; }
        json_t *ex = json_object();
        json_set(ex, "tool_progress", mode);
        json_t *r = tset_kv(ctx, rid, key, json_string(cur_focus ? "on" : "off"), ex);
        json_free(action); json_free(target); json_free(dc);
        return r;
    }
    bool target_truthy = tset_truthy(target);
    json_t *updates = json_object();
    char *effective = NULL;
    if (target_truthy) {
        /* saved = (cur_focus and d_f.get("focus_saved_tool_progress")) or _load_tool_progress_mode() */
        const json_t *saved = NULL;
        json_t *owned_saved = NULL;
        if (cur_focus) {
            const json_t *fsp = tset_dict_get(dc, "focus_saved_tool_progress", NULL, &gr);
            if (gr) { json_free(action); json_free(target); json_free(dc); json_free(updates); return gr; }
            saved = fsp;
        }
        if (!tset_truthy(saved)) {
            owned_saved = tset_call(ctx, "load_tool_progress_mode", tset_args0());
            if (tset_is_raise(owned_saved)) {
                json_free(action); json_free(target); json_free(dc); json_free(updates);
                return owned_saved;
            }
            saved = owned_saved;
        }
        json_t *na = tset_args0();
        tset_aset(na, "mode", saved);
        json_t *norm = tset_call(ctx, "normalize_tool_progress_mode", na);
        json_free(owned_saved);
        if (tset_is_raise(norm)) {
            json_free(action); json_free(target); json_free(dc); json_free(updates);
            return norm;
        }
        json_set(updates, "display.focus_saved_tool_progress", norm);
        effective = tset_strdup("off"); /* FOCUS_TOOL_PROGRESS_MODE */
    } else {
        const json_t *fsp = tset_dict_get(dc, "focus_saved_tool_progress", NULL, &gr);
        if (gr) { json_free(action); json_free(target); json_free(dc); json_free(updates); return gr; }
        json_t *na = tset_args0();
        if (tset_truthy(fsp)) tset_aset(na, "mode", fsp);
        else tset_asets(na, "mode", "all");
        json_t *norm = tset_call(ctx, "normalize_tool_progress_mode", na);
        if (tset_is_raise(norm)) {
            json_free(action); json_free(target); json_free(dc); json_free(updates);
            return norm;
        }
        effective = tset_pystr(norm);
        json_free(norm);
    }
    json_set(updates, "display.tool_progress", json_string(effective));
    json_set(updates, "display.focus_view", json_bool(target_truthy));
    json_t *wa = tset_args0();
    json_set(wa, "updates", updates);
    json_t *wk = tset_call(ctx, "write_config_keys", wa);
    if (tset_is_raise(wk)) {
        free(effective); json_free(action); json_free(target); json_free(dc);
        return wk;
    }
    json_free(wk);
    if (session) {
        json_set(session, "focus_view", json_bool(target_truthy));
        json_set(session, "tool_progress_mode", json_string(effective));
        const json_t *agent = tset_dict_get(session, "agent", NULL, &gr);
        if (gr) { free(effective); json_free(action); json_free(target); json_free(dc); return gr; }
        if (agent && agent->type != JSON_NULL) {
            /* with contextlib.suppress(Exception): agent.tool_progress_mode = effective */
            json_t *ar = tset_attr_set(agent, "tool_progress_mode", json_string(effective));
            if (ar) {
                const char *kind = tset_raise_kind(ar);
                if (tset_is_base_exc(kind)) {
                    free(effective); json_free(action); json_free(target); json_free(dc);
                    return ar;
                }
                json_free(ar); /* suppressed */
            }
        }
    }
    json_t *ex = json_object();
    json_set(ex, "tool_progress", json_string(effective));
    json_t *r = tset_kv(ctx, rid, key, json_string(target_truthy ? "on" : "off"), ex);
    free(effective); json_free(action); json_free(target); json_free(dc);
    return r;
}

/* -- _set_prompt -- */

/* PoP: tset_set_prompt @ tui_gateway/methods_config_set.py:_set_prompt */
static json_t *tset_set_prompt_body(tset_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *key = json_obj_get(args, "key");
    const json_t *value = json_obj_get(args, "value");
    json_t *kp = json_array();
    json_append(kp, json_string("custom_prompt"));
    json_t *rr = tset_refuse_managed_keys(ctx, kp);
    json_free(kp);
    if (rr) return rr;
    json_t *sft = tset_call(ctx, "supports_file_tooling", tset_args0());
    if (tset_is_raise(sft)) return sft;
    bool ft = tset_truthy(sft);
    json_free(sft);
    bool clear = value && value->type == JSON_STRING && value->str_val
                 && !strcmp(value->str_val, "clear");
    if (!ft) {
        json_t *sets = json_object();
        json_t *drops = json_array();
        if (clear) json_append(drops, json_string("custom_prompt"));
        else json_set(sets, "custom_prompt", value ? json_copy(value) : json_null());
        json_t *a = tset_args0();
        json_set(a, "sets", sets);
        json_set(a, "drops", drops);
        json_t *wc = tset_call(ctx, "write_config_changes", a);
        if (tset_is_raise(wc)) return wc;
        json_free(wc);
        return tset_kv(ctx, rid, key, clear ? json_string("") : json_copy(value), NULL);
    }
    json_t *cfg = tset_call(ctx, "load_cfg_raw", tset_args0());
    if (tset_is_raise(cfg)) return cfg;
    if (!cfg || cfg->type != JSON_OBJECT) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'pop'", tset_type_name(cfg));
        json_free(cfg);
        return tset_err_spec("AttributeError", msg);
    }
    if (clear) json_obj_del(cfg, "custom_prompt");
    else json_set(cfg, "custom_prompt", value ? json_copy(value) : json_null());
    json_t *a = tset_args0();
    json_set(a, "cfg", json_copy(cfg));
    json_t *sv = tset_call(ctx, "save_cfg", a);
    json_free(cfg);
    if (tset_is_raise(sv)) return sv;
    json_free(sv);
    return tset_kv(ctx, rid, key, clear ? json_string("") : json_copy(value), NULL);
}

static json_t *tset_set_prompt(tset_ctx_t *ctx, const json_t *args) {
    return tset_guard_catch(ctx, json_obj_get(args, "rid"), tset_set_prompt_body(ctx, args));
}

/* -- _set_personality -- */

/* PoP: tset_set_personality @ tui_gateway/methods_config_set.py:_set_personality */
static json_t *tset_set_personality_body(tset_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *key = json_obj_get(args, "key");
    const json_t *value = json_obj_get(args, "value");
    json_t *session = tset_session(ctx);
    json_t *kp = json_array();
    json_append(kp, json_string("display.personality"));
    json_t *rr = tset_refuse_managed_keys(ctx, kp);
    json_free(kp);
    if (rr) return rr;
    json_t *cfg = tset_call(ctx, "load_cfg_raw", tset_args0());
    if (tset_is_raise(cfg)) return cfg;
    char *vs = tset_truthy(value) ? tset_pystr(value) : tset_strdup("");
    json_t *va = tset_args0();
    tset_asets(va, "value", vs);
    json_set(va, "cfg", cfg); /* adopted */
    free(vs);
    json_t *vp = tset_call(ctx, "validate_personality", va);
    if (tset_is_raise(vp)) return vp;
    json_t *pname = NULL, *new_prompt = NULL;
    json_t *ur = tset_unpack2(vp, &pname, &new_prompt);
    json_free(vp);
    if (ur) return ur;
    json_t *pa = tset_args0();
    tset_aset(pa, "pname", pname);
    json_t *pp = tset_call(ctx, "persist_personality", pa);
    if (tset_is_raise(pp)) { json_free(pname); json_free(new_prompt); return pp; }
    json_free(pp);
    json_t *gr = NULL;
    const json_t *sid = tset_dict_get(json_obj_get(args, "params"), "session_id", NULL, &gr);
    if (gr) { json_free(pname); json_free(new_prompt); return gr; }
    json_t *aa = tset_args0();
    if (sid) tset_aset(aa, "sid", sid); else tset_asets(aa, "sid", "");
    tset_aset(aa, "session", session);
    tset_aset(aa, "prompt", new_prompt);
    tset_aset(aa, "pname", pname);
    json_t *ap = tset_call(ctx, "apply_personality_to_session", aa);
    json_free(pname); json_free(new_prompt);
    if (tset_is_raise(ap)) return ap;
    json_t *history_reset = NULL, *info = NULL;
    ur = tset_unpack2(ap, &history_reset, &info);
    json_free(ap);
    if (ur) return ur;
    json_t *ex = json_object();
    json_set(ex, "history_reset", history_reset);
    if (info && info->type != JSON_NULL) json_set(ex, "info", info);
    else json_free(info);
    char *rs = tset_truthy(value) ? tset_pystr(value) : tset_strdup("none");
    json_t *r = tset_kv(ctx, rid, key, json_string(rs), ex);
    free(rs);
    return r;
}

static json_t *tset_set_personality(tset_ctx_t *ctx, const json_t *args) {
    return tset_guard_catch(ctx, json_obj_get(args, "rid"), tset_set_personality_body(ctx, args));
}

/* -- _set_cwd -- */

/* PoP: tset_set_cwd @ tui_gateway/methods_config_set.py:_set_cwd */
static json_t *tset_set_cwd(tset_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *value = json_obj_get(args, "value");
    char *vs = tset_truthy(value) ? tset_pystr(value) : tset_strdup("");
    char *raw = tset_strip(vs);
    free(vs);
    if (!raw[0]) {
        free(raw);
        return tset_err(ctx, rid, 4002, "cwd required");
    }
    json_t *ea = tset_args0();
    tset_asets(ea, "raw", raw);
    json_t *eu = tset_call(ctx, "expanduser", ea);
    if (tset_is_raise(eu)) { free(raw); return eu; }
    json_t *aa = tset_args0();
    tset_aset(aa, "path", eu);
    json_t *cwd = tset_call(ctx, "abspath", aa);
    json_free(eu);
    if (tset_is_raise(cwd)) { free(raw); return cwd; }
    json_t *ia = tset_args0();
    tset_aset(ia, "path", cwd);
    json_t *isdir = tset_call(ctx, "isdir", ia);
    if (tset_is_raise(isdir)) { free(raw); json_free(cwd); return isdir; }
    bool exists = tset_truthy(isdir);
    json_free(isdir);
    if (!exists) {
        size_t n = strlen(raw) + 48;
        char *msg = malloc(n);
        if (msg) snprintf(msg, n, "working directory does not exist: %s", raw);
        free(raw); json_free(cwd);
        json_t *e = tset_err(ctx, rid, 4002, msg ? msg : "");
        free(msg);
        return e;
    }
    free(raw);
    json_t *wr = tset_write_config_key(ctx, "terminal.cwd", cwd);
    if (wr) { json_free(cwd); return wr; }
    /* Path(get_hermes_home()).resolve() == Path(_hermes_home).resolve() */
    json_t *gh = tset_call(ctx, "get_hermes_home", tset_args0());
    if (tset_is_raise(gh)) { json_free(cwd); return gh; }
    json_t *r1a = tset_args0();
    tset_aset(r1a, "path", gh);
    json_t *r1 = tset_call(ctx, "path_resolve", r1a);
    json_free(gh);
    if (tset_is_raise(r1)) { json_free(cwd); return r1; }
    const char *hh = json_get_str(args, "hermes_home", "/h/launch");
    json_t *r2a = tset_args0();
    tset_asets(r2a, "path", hh);
    json_t *r2 = tset_call(ctx, "path_resolve", r2a);
    if (tset_is_raise(r2)) { json_free(cwd); json_free(r1); return r2; }
    const char *s1 = tset_str(r1);
    const char *s2 = tset_str(r2);
    bool same = s1 && s2 && !strcmp(s1, s2);
    json_free(r1); json_free(r2);
    if (same) {
        json_t *esa = tset_args0();
        tset_asets(esa, "key", "TERMINAL_CWD");
        tset_aset(esa, "value", cwd);
        json_t *es = tset_call(ctx, "environ_set", esa);
        if (tset_is_raise(es)) { json_free(cwd); return es; }
        json_free(es);
    }
    json_t *ga = tset_args0();
    tset_aset(ga, "cwd", cwd);
    json_t *branch = tset_call(ctx, "git_branch", ga);
    if (tset_is_raise(branch)) { json_free(cwd); return branch; }
    json_t *ex = json_object();
    json_set(ex, "cwd", json_copy(cwd));
    json_set(ex, "branch", branch);
    json_t *r = tset_kv(ctx, rid, json_string("terminal.cwd"), cwd, ex);
    return r;
}

/* -- _set_yolo + persist closure -- */

/* PoP: tset_set_yolo_persist @ tui_gateway/methods_config_set.py:_set_yolo */
static json_t *tset_set_yolo_persist(tset_ctx_t *ctx, const json_t *args) {
    json_t *session = tset_session(ctx);
    json_t *sa = tset_args0();
    tset_aset(sa, "session", session);
    json_t *db = tset_call(ctx, "session_db", sa);
    if (tset_is_raise(db)) return db;
    json_t *pa = tset_args0();
    tset_aset(pa, "db", db);
    json_t *persistence = tset_call(ctx, "persistence_for", pa);
    json_free(db);
    if (tset_is_raise(persistence)) return persistence;
    if (persistence && persistence->type != JSON_NULL) {
        json_t *ya = tset_args0();
        tset_aset(ya, "row_id", json_obj_get(args, "row_id"));
        tset_aset(ya, "on", json_obj_get(args, "on"));
        json_t *sy = tset_call(ctx, "set_yolo", ya);
        json_free(persistence);
        if (tset_is_raise(sy)) return sy;
        json_free(sy);
        return NULL;
    }
    json_free(persistence);
    return NULL;
}

/* PoP: tset_set_yolo @ tui_gateway/methods_config_set.py:_set_yolo */
static json_t *tset_set_yolo_body(tset_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *key = json_obj_get(args, "key");
    const json_t *value = json_obj_get(args, "value");
    json_t *session = tset_session(ctx);
    const json_t *params = json_obj_get(args, "params");
    json_t *gr = NULL;
    const json_t *scope_v = tset_dict_get(params, "scope", NULL, &gr);
    if (gr) return gr;
    json_t *scope_def = json_string("session");
    char *scope = tset_word_v(tset_truthy(scope_v) ? scope_v : scope_def);
    json_free(scope_def);
    char *raw = tset_word_v(value);
    json_t *enable = NULL;
    if (!strcmp(scope, "global")) {
        json_t *cfg = tset_call(ctx, "load_cfg", tset_args0());
        if (tset_is_raise(cfg)) { free(scope); free(raw); return cfg; }
        const json_t *appr0 = tset_dict_get(cfg, "approvals", NULL, &gr);
        if (gr) { json_free(cfg); free(scope); free(raw); return gr; }
        json_t *appr = (appr0 && appr0->type == JSON_OBJECT) ? json_copy(appr0) : json_object();
        json_free(cfg);
        const json_t *mode = tset_dict_get(appr, "mode", NULL, &gr);
        if (gr) { json_free(appr); free(scope); free(raw); return gr; }
        json_t *na = tset_args0();
        if (mode) tset_aset(na, "mode", mode); else tset_asets(na, "mode", "manual");
        json_t *nm = tset_call(ctx, "normalize_approval_mode", na);
        json_free(appr);
        if (tset_is_raise(nm)) { free(scope); free(raw); return nm; }
        const char *nms = tset_str(nm);
        bool def_enable = !(nms && !strcmp(nms, "off"));
        json_free(nm);
        enable = tset_bool_word(raw);
        if (!enable) enable = json_bool(def_enable);
        json_t *wr = tset_write_config_key(ctx, "approvals.mode",
                                           json_string(tset_truthy(enable) ? "off" : "manual"));
        if (wr) { json_free(enable); free(scope); free(raw); return wr; }
        json_t *ea = tset_emit_all_session_info(ctx);
        if (ea) { json_free(enable); free(scope); free(raw); return ea; }
    } else if (session) {
        const json_t *agent = tset_dict_get(session, "agent", NULL, &gr);
        if (gr) { free(scope); free(raw); return gr; }
        const json_t *asid = tset_attr(agent, "session_id", true, NULL, &gr);
        if (gr) { free(scope); free(raw); return gr; }
        json_t *row_id = NULL;
        if (tset_truthy(asid)) row_id = json_copy(asid);
        else {
            json_t *sr = NULL;
            const json_t *sk = tset_subscript(session, "session_key", &sr);
            if (sr) { free(scope); free(raw); return sr; }
            row_id = json_copy(sk);
        }
        json_free(row_id);
        json_t *sr = NULL;
        const json_t *sk = tset_subscript(session, "session_key", &sr);
        if (sr) { free(scope); free(raw); return sr; }
        json_t *ta = tset_args0();
        tset_aset(ta, "session_key", sk);
        tset_aset(ta, "value", tset_bool_word(raw));
        tset_asets(ta, "persist", "persist");
        enable = tset_call(ctx, "toggle_session_yolo", ta);
        if (tset_is_raise(enable)) { free(scope); free(raw); return enable; }
        const json_t *sid = tset_dict_get(params, "session_id", NULL, &gr);
        if (gr) { json_free(enable); free(scope); free(raw); return gr; }
        json_t *ei = tset_emit_session_info(ctx, sid, session);
        if (ei) { json_free(enable); free(scope); free(raw); return ei; }
    } else {
        json_t *ega = tset_args0();
        tset_asets(ega, "key", "HERMES_YOLO_MODE");
        json_t *ev = tset_call(ctx, "environ_get", ega);
        if (tset_is_raise(ev)) { free(scope); free(raw); return ev; }
        json_t *ita = tset_args0();
        tset_aset(ita, "value", ev);
        json_t *tv = tset_call(ctx, "is_truthy_value", ita);
        json_free(ev);
        if (tset_is_raise(tv)) { free(scope); free(raw); return tv; }
        bool def_enable = !tset_truthy(tv);
        json_free(tv);
        enable = tset_bool_word(raw);
        if (!enable) enable = json_bool(def_enable);
        json_t *er = NULL;
        if (tset_truthy(enable)) {
            json_t *esa = tset_args0();
            tset_asets(esa, "key", "HERMES_YOLO_MODE");
            tset_asets(esa, "value", "1");
            er = tset_call(ctx, "environ_set", esa);
        } else {
            json_t *epa = tset_args0();
            tset_asets(epa, "key", "HERMES_YOLO_MODE");
            er = tset_call(ctx, "environ_pop", epa);
        }
        if (tset_is_raise(er)) { json_free(enable); free(scope); free(raw); return er; }
        json_free(er);
    }
    json_t *ex = json_object();
    json_set_hdr(ex, "scope", !strcmp(scope, "global") ? "global" : "session");
    json_t *r = tset_kv(ctx, rid, key, json_string(tset_truthy(enable) ? "1" : "0"), ex);
    json_free(enable); free(scope); free(raw);
    return r;
}

static json_t *tset_set_yolo(tset_ctx_t *ctx, const json_t *args) {
    return tset_guard_catch(ctx, json_obj_get(args, "rid"), tset_set_yolo_body(ctx, args));
}

/* -- _set_reasoning -- */

/* PoP: tset_set_reasoning @ tui_gateway/methods_config_set.py:_set_reasoning */
static json_t *tset_set_reasoning_body(tset_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *key = json_obj_get(args, "key");
    const json_t *value = json_obj_get(args, "value");
    json_t *session = tset_session(ctx);
    const json_t *params = json_obj_get(args, "params");
    char *arg = tset_word_v(value);
    json_t *gr = NULL;
    const json_t *scope_v = tset_dict_get(params, "scope", NULL, &gr);
    if (gr) { free(arg); return gr; }
    char *scope = tset_word_v(scope_v);
    /* _REASONING_DISPLAY_WORDS rows */
    static const char *const words0[] = {"show", "on", NULL};
    static const char *const words1[] = {"hide", "off", NULL};
    static const char *const words2[] = {"full", "all", NULL};
    static const char *const words3[] = {"clamp", "collapse", "short", NULL};
    const char *const *words[4] = {words0, words1, words2, words3};
    static const char *const reported[4] = {"show", "hide", "full", "clamp"};
    static const char *const field_key[4] = {"show_reasoning", "show_reasoning",
                                             "reasoning_full", "reasoning_full"};
    static const bool field_val[4] = {true, false, true, false};
    static const char *const thinking[4] = {"expanded", "hidden", "expanded", "collapsed"};
    static const int show[4] = {1, 0, -1, -1}; /* 1=True 0=False -1=None */
    for (int i = 0; i < 4; i++) {
        if (tset_in_set(arg, words[i])) {
            json_t *sections = json_object();
            json_set(sections, "thinking", json_string(thinking[i]));
            json_t *fields = json_object();
            json_set(fields, field_key[i], json_bool(field_val[i]));
            json_t *wr = tset_write_display_sections(ctx, sections, NULL, fields);
            json_free(sections); json_free(fields);
            if (wr) { free(arg); free(scope); return wr; }
            if (show[i] >= 0 && session)
                json_set(session, "show_reasoning", json_bool(show[i] == 1));
            json_t *r = tset_kv(ctx, rid, key, json_string(reported[i]), NULL);
            free(arg); free(scope);
            return r;
        }
    }
    json_t *pa = tset_args0();
    tset_asets(pa, "raw", arg);
    json_t *parsed = tset_call(ctx, "parse_reasoning_effort", pa);
    if (tset_is_raise(parsed)) { free(arg); free(scope); return parsed; }
    if (!parsed || parsed->type == JSON_NULL) {
        json_free(parsed);
        char *vs = tset_pystr(value);
        size_t n = strlen(vs) + 40;
        char *msg = malloc(n);
        if (msg) snprintf(msg, n, "unknown reasoning value: %s", vs);
        free(vs); free(arg); free(scope);
        json_t *e = tset_err(ctx, rid, 4002, msg ? msg : "");
        free(msg);
        return e;
    }
    if (!strcmp(scope, "global") || !session) {
        json_t *wr = tset_write_config_key(ctx, "agent.reasoning_effort", json_string(arg));
        if (wr) { json_free(parsed); free(arg); free(scope); return wr; }
        if (session) json_obj_del(session, "create_reasoning_override");
    } else {
        json_set(session, "create_reasoning_override", json_copy(parsed));
    }
    if (session) {
        const json_t *agent = tset_dict_get(session, "agent", NULL, &gr);
        if (gr) { json_free(parsed); free(arg); free(scope); return gr; }
        if (agent && agent->type != JSON_NULL) {
            json_t *ar = tset_attr_set(agent, "reasoning_config", json_copy(parsed));
            if (ar) { json_free(parsed); free(arg); free(scope); return ar; }
            json_t *pra = tset_args0();
            tset_aset(pra, "session", session);
            json_t *pl = tset_call(ctx, "persist_live_session_runtime", pra);
            if (tset_is_raise(pl)) { json_free(parsed); free(arg); free(scope); return pl; }
            json_free(pl);
            const json_t *sid = tset_dict_get(params, "session_id", NULL, &gr);
            if (gr) { json_free(parsed); free(arg); free(scope); return gr; }
            json_t *ei = tset_emit_session_info(ctx, sid, session);
            if (ei) { json_free(parsed); free(arg); free(scope); return ei; }
        }
    }
    json_t *r = tset_kv(ctx, rid, key, json_string(arg), NULL);
    json_free(parsed); free(arg); free(scope);
    return r;
}

static json_t *tset_set_reasoning(tset_ctx_t *ctx, const json_t *args) {
    return tset_guard_catch(ctx, json_obj_get(args, "rid"), tset_set_reasoning_body(ctx, args));
}

/* -- _set_fast -- */

/* _FAST_WORDS.get(word) -> fresh string node or NULL. */
static json_t *tset_fast_word(const char *w) {
    if (!w) return NULL;
    static const char *const keys[] = {"fast", "on", "normal", "off", "auto", "cold",
                                       "ultrafast", "ultra", NULL};
    static const char *const vals[] = {"fast", "fast", "normal", "normal", "auto", "cold",
                                       "ultrafast", "ultrafast", NULL};
    for (size_t i = 0; keys[i]; i++) if (!strcmp(w, keys[i])) return json_string(vals[i]);
    return NULL;
}

/* PoP: tset_set_fast @ tui_gateway/methods_config_set.py:_set_fast */
static json_t *tset_set_fast(tset_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *key = json_obj_get(args, "key");
    const json_t *value = json_obj_get(args, "value");
    json_t *session = tset_session(ctx);
    const json_t *params = json_obj_get(args, "params");
    char *raw = tset_word_v(value);
    json_t *gr = NULL;
    const json_t *agent = NULL;
    if (session) {
        agent = tset_dict_get(session, "agent", NULL, &gr);
        if (gr) { free(raw); return gr; }
    }
    bool agent_present = agent && agent->type != JSON_NULL;
    /* current_tier */
    json_t *current_tier = NULL;
    bool owns_tier = false;
    if (agent_present) {
        const json_t *st = tset_attr(agent, "service_tier", true, NULL, &gr);
        if (gr) { free(raw); return gr; }
        current_tier = st ? json_copy(st) : json_null();
        owns_tier = true;
    } else if (session) {
        const json_t *ov = tset_dict_get(session, "create_service_tier_override", NULL, &gr);
        if (gr) { free(raw); return gr; }
        if (ov && ov->type != JSON_NULL) {
            current_tier = tset_truthy(ov) ? json_copy(ov) : json_null();
            owns_tier = true;
        }
    }
    if (!owns_tier) {
        current_tier = tset_call(ctx, "load_service_tier", tset_args0());
        if (tset_is_raise(current_tier)) { free(raw); return current_tier; }
        owns_tier = true;
    }
    if (!strcmp(raw, "status")) {
        json_t *sa = tset_args0();
        tset_aset(sa, "tier", current_tier);
        json_t *word = tset_call(ctx, "service_tier_word", sa);
        if (owns_tier) json_free(current_tier);
        free(raw);
        if (tset_is_raise(word)) return word;
        return tset_kv(ctx, rid, key, word, NULL);
    }
    /* nv = _FAST_WORDS.get(raw, default) */
    json_t *nv = tset_fast_word(raw);
    if (!nv) {
        if (!strcmp(raw, "") || !strcmp(raw, "toggle")) {
            const char *ts = tset_str(current_tier);
            nv = json_string(tset_in_set(ts, TSET_STATIC_TIERS) ? "normal" : "fast");
        }
    }
    if (!nv || nv->type == JSON_NULL) {
        if (nv) json_free(nv);
        if (owns_tier) json_free(current_tier);
        char *vs = tset_pystr(value);
        size_t n = strlen(vs) + 40;
        char *msg = malloc(n);
        if (msg) snprintf(msg, n, "unknown fast mode: %s", vs);
        free(vs); free(raw);
        json_t *e = tset_err(ctx, rid, 4002, msg ? msg : "");
        free(msg);
        return e;
    }
    char *nvs = tset_pystr(nv);
    json_t *overrides = NULL;
    if (!strcmp(nvs, "fast") || !strcmp(nvs, "ultrafast")) {
        json_t *target_model = NULL;
        bool owns_tm = false;
        if (agent_present) {
            const json_t *m = tset_attr(agent, "model", true, NULL, &gr);
            if (gr) { json_free(nv); free(nvs); if (owns_tier) json_free(current_tier); free(raw); return gr; }
            target_model = m ? json_copy(m) : json_null();
            owns_tm = true;
        } else {
            const json_t *so0 = session ? tset_dict_get(session, "model_override", NULL, &gr) : NULL;
            if (gr) { json_free(nv); free(nvs); if (owns_tier) json_free(current_tier); free(raw); return gr; }
            const json_t *so = tset_truthy(so0) ? so0 : NULL;
            const json_t *tm_cand = NULL;
            if (so && so->type == JSON_OBJECT) tm_cand = json_obj_get(so, "model");
            if (tset_truthy(tm_cand)) { target_model = json_copy(tm_cand); owns_tm = true; }
            else {
                target_model = tset_call(ctx, "resolve_model", tset_args0());
                owns_tm = true;
                if (tset_is_raise(target_model)) {
                    json_free(nv); free(nvs); if (owns_tier) json_free(current_tier); free(raw);
                    return target_model;
                }
            }
        }
        if (!tset_truthy(target_model)) {
            if (owns_tm) json_free(target_model);
            json_free(nv); free(nvs); if (owns_tier) json_free(current_tier); free(raw);
            return tset_err(ctx, rid, 4002, "fast mode is not available without a selected model");
        }
        const json_t *prov = tset_attr(agent, "provider", true, NULL, &gr);
        if (gr) { if (owns_tm) json_free(target_model); json_free(nv); free(nvs); if (owns_tier) json_free(current_tier); free(raw); return gr; }
        const json_t *burl = tset_attr(agent, "base_url", true, NULL, &gr);
        if (gr) { if (owns_tm) json_free(target_model); json_free(nv); free(nvs); if (owns_tier) json_free(current_tier); free(raw); return gr; }
        json_t *oa = tset_args0();
        tset_aset(oa, "model", target_model);
        tset_aset(oa, "provider", prov);
        tset_aset(oa, "base_url", burl);
        if (!strcmp(nvs, "ultrafast")) tset_asets(oa, "tier", "ultrafast");
        overrides = tset_call(ctx, "resolve_fast_mode_overrides", oa);
        if (tset_is_raise(overrides)) {
            if (owns_tm) json_free(target_model);
            json_free(nv); free(nvs); if (owns_tier) json_free(current_tier); free(raw);
            return overrides;
        }
        if (overrides && overrides->type != JSON_NULL && !strcmp(nvs, "ultrafast")) {
            const json_t *apikey = tset_attr(agent, "api_key", true, NULL, &gr);
            if (gr) { json_free(overrides); if (owns_tm) json_free(target_model); json_free(nv); free(nvs); if (owns_tier) json_free(current_tier); free(raw); return gr; }
            json_t *ua = tset_args0();
            tset_aset(ua, "model", target_model);
            tset_aset(ua, "provider", prov);
            tset_aset(ua, "base_url", burl);
            tset_aset(ua, "access_token", apikey);
            json_t *offered = tset_call(ctx, "ultrafast_offered", ua);
            if (tset_is_raise(offered)) {
                json_free(overrides); if (owns_tm) json_free(target_model);
                json_free(nv); free(nvs); if (owns_tier) json_free(current_tier); free(raw);
                return offered;
            }
            if (!tset_truthy(offered)) { json_free(overrides); overrides = NULL; }
            json_free(offered);
        }
        if (owns_tm) json_free(target_model);
        if (!overrides || overrides->type == JSON_NULL) {
            if (overrides) json_free(overrides);
            size_t n = strlen(nvs) + 48;
            char *msg = malloc(n);
            if (msg) snprintf(msg, n, "%s mode is not available for this model", nvs);
            json_free(nv); free(nvs); if (owns_tier) json_free(current_tier); free(raw);
            json_t *e = tset_err(ctx, rid, 4002, msg ? msg : "");
            free(msg);
            return e;
        }
    }
    if (session) {
        const char *pin = !strcmp(nvs, "fast") ? "priority" : !strcmp(nvs, "normal") ? "" : nvs;
        json_set(session, "create_service_tier_override", json_string(pin));
    } else {
        json_t *wr = tset_write_config_key(ctx, "agent.service_tier", nv);
        if (wr) { json_free(overrides); json_free(nv); free(nvs); if (owns_tier) json_free(current_tier); free(raw); return wr; }
    }
    if (agent_present) {
        json_t *tier_val = !strcmp(nvs, "fast") ? json_string("priority")
                         : !strcmp(nvs, "normal") ? json_null() : json_copy(nv);
        json_t *ar = tset_attr_set(agent, "service_tier", tier_val);
        if (ar) { json_free(overrides); json_free(nv); free(nvs); if (owns_tier) json_free(current_tier); free(raw); return ar; }
        /* current_overrides = {k:v for (getattr(agent,"request_overrides",{}) or {}).items()
         *                      if k not in ("service_tier","speed")} */
        const json_t *ro0 = tset_attr(agent, "request_overrides", true, NULL, &gr);
        if (gr) { json_free(overrides); json_free(nv); free(nvs); if (owns_tier) json_free(current_tier); free(raw); return gr; }
        const json_t *ro = tset_truthy(ro0) ? ro0 : NULL;
        json_t *merged = json_object();
        if (ro && ro->type != JSON_OBJECT) {
            char msg[160];
            snprintf(msg, sizeof msg, "'%s' object has no attribute 'items'", tset_type_name(ro));
            json_free(merged); json_free(overrides); json_free(nv); free(nvs);
            if (owns_tier) json_free(current_tier);
            free(raw);
            return tset_err_spec("AttributeError", msg);
        }
        if (ro) {
            for (size_t i = 0; i < ro->c.count; i++) {
                if (!strcmp(ro->c.keys[i], "service_tier") || !strcmp(ro->c.keys[i], "speed")) continue;
                json_set(merged, ro->c.keys[i], json_copy(ro->c.items[i]));
            }
        }
        if (overrides && overrides->type == JSON_OBJECT) {
            for (size_t i = 0; i < overrides->c.count; i++)
                json_set(merged, overrides->c.keys[i], json_copy(overrides->c.items[i]));
        } else if (overrides && overrides->type != JSON_NULL) {
            json_t *me = tset_mapping_error(overrides);
            json_free(merged); json_free(overrides); json_free(nv); free(nvs);
            if (owns_tier) json_free(current_tier);
            free(raw);
            return me;
        }
        ar = tset_attr_set(agent, "request_overrides", merged);
        if (ar) { json_free(overrides); json_free(nv); free(nvs); if (owns_tier) json_free(current_tier); free(raw); return ar; }
        json_t *pra = tset_args0();
        tset_aset(pra, "session", session);
        json_t *pl = tset_call(ctx, "persist_live_session_runtime", pra);
        if (tset_is_raise(pl)) { json_free(overrides); json_free(nv); free(nvs); if (owns_tier) json_free(current_tier); free(raw); return pl; }
        json_free(pl);
        const json_t *sid = tset_dict_get(params, "session_id", NULL, &gr);
        if (gr) { json_free(overrides); json_free(nv); free(nvs); if (owns_tier) json_free(current_tier); free(raw); return gr; }
        json_t *ei = tset_emit_session_info(ctx, sid, session);
        if (ei) { json_free(overrides); json_free(nv); free(nvs); if (owns_tier) json_free(current_tier); free(raw); return ei; }
    }
    json_t *r = tset_kv(ctx, rid, key, nv, NULL);
    json_free(overrides);
    free(nvs);
    if (owns_tier) json_free(current_tier);
    free(raw);
    return r;
}

/* -- _set_model -- */

/* PoP: tset_set_model @ tui_gateway/methods_config_set.py:_set_model */
static json_t *tset_set_model_body(tset_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *key = json_obj_get(args, "key");
    const json_t *value = json_obj_get(args, "value");
    json_t *session = tset_session(ctx);
    const json_t *params = json_obj_get(args, "params");
    if (!tset_truthy(value))
        return tset_err(ctx, rid, 4002, "model value required");
    json_t *gr = NULL;
    const json_t *cfm = tset_dict_get(params, "confirm_expensive_model", NULL, &gr);
    if (gr) return gr;
    bool confirmed = tset_truthy(cfm);
    json_t *result = NULL;
    if (session) {
        const json_t *sid = tset_dict_get(params, "session_id", NULL, &gr);
        if (gr) return gr;
        json_t *pfa = tset_args0();
        tset_aset(pfa, "value", value);
        json_t *parsed_flags = tset_call(ctx, "parse_model_switch_args", pfa);
        if (tset_is_raise(parsed_flags)) return parsed_flags;
        const json_t *running = tset_dict_get(session, "running", NULL, &gr);
        if (gr) { json_free(parsed_flags); return gr; }
        const json_t *cha = tset_dict_get(session, "_compute_host_active", NULL, &gr);
        if (gr) { json_free(parsed_flags); return gr; }
        if (tset_truthy(running) || tset_truthy(cha)) {
            json_t *r = tset_stash_pending_model_switch(ctx, rid, key, value, session,
                                                        confirmed, parsed_flags);
            json_free(parsed_flags);
            return r;
        }
        const json_t *ep = tset_attr(parsed_flags, "explicit_provider", false, NULL, &gr);
        if (gr) { json_free(parsed_flags); return gr; }
        const json_t *agent = tset_dict_get(session, "agent", NULL, &gr);
        if (gr) { json_free(parsed_flags); return gr; }
        const json_t *aerr = tset_dict_get(session, "agent_error", NULL, &gr);
        if (gr) { json_free(parsed_flags); return gr; }
        bool agent_none = !agent || agent->type == JSON_NULL;
        bool err_present = aerr && aerr->type != JSON_NULL;
        bool fai = agent_none && err_present;
        const json_t *failed_ready = NULL;
        if (fai) {
            failed_ready = tset_dict_get(session, "agent_ready", NULL, &gr);
            if (gr) { json_free(parsed_flags); return gr; }
        }
        if (fai) {
            if (!failed_ready || failed_ready->type == JSON_NULL) {
                char *es = tset_truthy(aerr) ? tset_pystr(aerr) : tset_strdup("agent initialization failed");
                json_free(parsed_flags);
                json_t *e = tset_err(ctx, rid, 5032, es);
                free(es);
                return e;
            }
            json_t *wa = tset_args0();
            tset_asets(wa, "id", json_get_str(failed_ready, "id", ""));
            tset_asetn(wa, "timeout", 30.0);
            json_t *waited = tset_call(ctx, "event_wait", wa);
            if (tset_is_raise(waited)) { json_free(parsed_flags); return waited; }
            bool ok = tset_truthy(waited);
            json_free(waited);
            if (!ok) {
                json_free(parsed_flags);
                return tset_err(ctx, rid, 5032, TSET_AGENT_STILL_STARTING);
            }
        }
        /* re-evaluate failed_agent_init */
        const json_t *agent2 = tset_dict_get(session, "agent", NULL, &gr);
        if (gr) { json_free(parsed_flags); return gr; }
        const json_t *aerr2 = tset_dict_get(session, "agent_error", NULL, &gr);
        if (gr) { json_free(parsed_flags); return gr; }
        const json_t *ready2 = tset_dict_get(session, "agent_ready", NULL, &gr);
        if (gr) { json_free(parsed_flags); return gr; }
        bool agent_none2 = !agent2 || agent2->type == JSON_NULL;
        bool err_present2 = aerr2 && aerr2->type != JSON_NULL;
        bool ready_identity = false;
        if (failed_ready && ready2) {
            const char *i1 = json_get_str(failed_ready, "id", "");
            const char *i2 = json_get_str(ready2, "id", "");
            ready_identity = !strcmp(i1, i2);
        } else if (!failed_ready && (!ready2 || ready2->type == JSON_NULL)) {
            ready_identity = true;
        }
        bool is_set = false;
        if (fai && ready_identity && failed_ready && failed_ready->type != JSON_NULL) {
            json_t *isa = tset_args0();
            tset_asets(isa, "id", json_get_str(failed_ready, "id", ""));
            json_t *isset = tset_call(ctx, "event_is_set", isa);
            if (tset_is_raise(isset)) { json_free(parsed_flags); return isset; }
            is_set = tset_truthy(isset);
            json_free(isset);
        }
        bool fai2 = fai && agent_none2 && err_present2 && ready_identity && is_set;
        char *eps = tset_pystr(ep);
        char *epst = tset_strip(eps);
        free(eps);
        bool ep_empty = !epst[0];
        free(epst);
        const json_t *agent3 = tset_dict_get(session, "agent", NULL, &gr);
        if (gr) { json_free(parsed_flags); return gr; }
        bool agent_none3 = !agent3 || agent3->type == JSON_NULL;
        if (agent_none3 && ep_empty && !fai2) {
            json_t *sba = tset_args0();
            if (sid) tset_aset(sba, "sid", sid); else tset_asets(sba, "sid", "");
            tset_aset(sba, "session", session);
            json_t *sb = tset_call(ctx, "start_agent_build", sba);
            if (tset_is_raise(sb)) { json_free(parsed_flags); return sb; }
            json_free(sb);
            json_t *init_err = tset_cfgset_await_agent(ctx, session, rid);
            if (init_err) { json_free(parsed_flags); return init_err; }
        }
        /* with _session_profile_runtime_scope(session): result = _apply_model_switch(...) */
        json_t *sca = tset_args0();
        tset_aset(sca, "session", session);
        json_t *scope = tset_call(ctx, "session_profile_runtime_scope", sca);
        if (tset_is_raise(scope)) { json_free(parsed_flags); return scope; }
        if (!scope || scope->type != JSON_OBJECT) {
            json_t *ce = tset_cm_error(scope);
            json_free(scope); json_free(parsed_flags);
            return ce;
        }
        json_free(scope);
        json_t *ama = tset_args0();
        if (sid) tset_aset(ama, "sid", sid); else tset_asets(ama, "sid", "");
        tset_aset(ama, "session", session);
        tset_aset(ama, "value", value);
        tset_asetb(ama, "confirm_expensive_model", confirmed);
        tset_aset(ama, "parsed_flags", parsed_flags);
        result = tset_call(ctx, "apply_model_switch", ama);
        if (tset_is_raise(result)) { json_free(parsed_flags); return result; }
        if (fai2) {
            const json_t *cr = tset_dict_get(result, "confirm_required", NULL, &gr);
            if (gr) { json_free(result); json_free(parsed_flags); return gr; }
            if (!tset_truthy(cr)) {
                json_t *ra = tset_args0();
                if (sid) tset_aset(ra, "sid", sid); else tset_asets(ra, "sid", "");
                tset_aset(ra, "session", session);
                tset_aset(ra, "ready", failed_ready);
                json_t *restart = tset_call(ctx, "restart_failed_agent_build", ra);
                if (tset_is_raise(restart)) { json_free(result); json_free(parsed_flags); return restart; }
                json_free(restart);
                json_t *init_err = tset_cfgset_await_agent(ctx, session, rid);
                if (init_err) { json_free(result); json_free(parsed_flags); return init_err; }
                json_t *sc2a = tset_args0();
                tset_aset(sc2a, "session", session);
                json_t *scope2 = tset_call(ctx, "session_profile_runtime_scope", sc2a);
                if (tset_is_raise(scope2)) { json_free(result); json_free(parsed_flags); return scope2; }
                if (!scope2 || scope2->type != JSON_OBJECT) {
                    json_t *ce = tset_cm_error(scope2);
                    json_free(scope2); json_free(result); json_free(parsed_flags);
                    return ce;
                }
                json_free(scope2);
                json_t *pla = tset_args0();
                tset_aset(pla, "session", session);
                json_t *pl = tset_call(ctx, "persist_live_session_runtime", pla);
                if (tset_is_raise(pl)) { json_free(result); json_free(parsed_flags); return pl; }
                json_free(pl);
            }
        }
        json_free(parsed_flags);
    } else {
        char *vs = tset_pystr(value);
        json_t *pfa = tset_args0();
        tset_asets(pfa, "value", vs);
        free(vs);
        json_t *parsed = tset_call(ctx, "parse_model_switch_args", pfa);
        if (tset_is_raise(parsed)) return parsed;
        const json_t *once = tset_attr(parsed, "is_once", false, NULL, &gr);
        if (gr) { json_free(parsed); return gr; }
        bool is_once = tset_truthy(once);
        json_free(parsed);
        if (is_once) {
            json_t *sess_arg = json_object();
            json_set(sess_arg, "agent", json_null());
            json_t *ama = tset_args0();
            tset_asets(ama, "sid", "");
            json_set(ama, "session", sess_arg);
            tset_aset(ama, "value", value);
            tset_asetb(ama, "confirm_expensive_model", confirmed);
            result = tset_call(ctx, "apply_model_switch", ama);
            if (tset_is_raise(result)) return result;
        } else {
            return tset_err(ctx, rid, 4001,
                "config.set model requires a live session; to change the "
                "profile default run /setup, or use the Models page (dashboard) / "
                "Settings -> Models (Desktop)");
        }
    }
    /* return _kv(rid, key, result["value"], warning=result["warning"], ...) */
    json_t *sr = NULL;
    const json_t *rval = tset_subscript(result, "value", &sr);
    if (sr) { json_free(result); return sr; }
    const json_t *rwarn = tset_subscript(result, "warning", &sr);
    if (sr) { json_free(result); return sr; }
    const json_t *cr = tset_dict_get(result, "confirm_required", NULL, &gr);
    if (gr) { json_free(result); return gr; }
    const json_t *cm = tset_dict_get(result, "confirm_message", NULL, &gr);
    if (gr) { json_free(result); return gr; }
    const json_t *sc = tset_dict_get(result, "scope", NULL, &gr);
    if (gr) { json_free(result); return gr; }
    json_t *ex = json_object();
    tset_aset(ex, "warning", rwarn);
    json_set(ex, "confirm_required", json_bool(tset_truthy(cr)));
    if (cm) tset_aset(ex, "confirm_message", cm); else tset_asets(ex, "confirm_message", "");
    if (sc) tset_aset(ex, "scope", sc); else tset_asets(ex, "scope", "session");
    json_t *r = tset_kv(ctx, rid, key, rval, ex);
    json_free(result);
    return r;
}

static json_t *tset_set_model(tset_ctx_t *ctx, const json_t *args) {
    return tset_guard_catch(ctx, json_obj_get(args, "rid"), tset_set_model_body(ctx, args));
}

/* -- _h_config_set (the config.set dispatcher, decorators stripped) -- */

typedef json_t *(*tset_handler_t)(tset_ctx_t *, const json_t *);
typedef struct { const char *key; tset_handler_t fn; } tset_setter_t;

static const tset_setter_t TSET_SETTERS[] = {
    {"model", tset_set_model}, {"fast", tset_set_fast}, {"busy", tset_set_busy},
    {"verbose", tset_set_verbose}, {"focus", tset_set_focus},
    {"approval_mode", tset_set_approval_mode}, {"approvals.mode", tset_set_word_case},
    {"yolo", tset_set_yolo}, {"reasoning", tset_set_reasoning},
    {"details_mode", tset_set_word_case}, {"thinking_mode", tset_set_word_case},
    {"density", tset_set_toggle_case}, {"battery", tset_set_toggle_case},
    {"timestamps", tset_set_toggle_case}, {"theme", tset_set_word_case},
    {"statusbar", tset_set_toggle_case}, {"mouse", tset_set_toggle_case},
    {"indicator", tset_set_word_case}, {"voice.voice_chat_mode", tset_set_word_case},
    {"cwd", tset_set_cwd}, {"terminal.cwd", tset_set_cwd}, {"workdir", tset_set_cwd},
    {"prompt", tset_set_prompt}, {"personality", tset_set_personality},
    {"skin", tset_set_skin}, {"daybreak", tset_set_daybreak},
};
#define TSET_SETTER_COUNT (sizeof(TSET_SETTERS) / sizeof(TSET_SETTERS[0]))

/* PoP: tset_h_config_set @ tui_gateway/methods_config_set.py:_ */
static json_t *tset_h_config_set(tset_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    json_t *gr = NULL;
    const json_t *key = tset_dict_get(params, "key", NULL, &gr);
    if (gr) return gr;
    const json_t *value = tset_dict_get(params, "value", NULL, &gr);
    if (gr) return gr;
    const char *k = tset_str(key);
    if (!k) k = "";
    const json_t *sid_v = tset_dict_get(params, "session_id", NULL, &gr);
    if (gr) return gr;
    json_t *sga = tset_args0();
    if (sid_v) tset_aset(sga, "session_id", sid_v); else tset_asets(sga, "session_id", "");
    json_t *sess_ret = tset_call(ctx, "sessions_get", sga);
    if (tset_is_raise(sess_ret)) return sess_ret;
    bool sess_present = sess_ret && sess_ret->type != JSON_NULL;
    json_free(sess_ret);
    json_t *carrier = json_obj_get(ctx->state, "session");
    json_t *session = (sess_present && carrier && carrier->type == JSON_OBJECT) ? carrier : NULL;
    /* stale session_id guard */
    const json_t *scope_v = tset_dict_get(params, "scope", NULL, &gr);
    if (gr) return gr;
    char *scope = tset_word_v(scope_v);
    bool stale = !session && tset_truthy(sid_v) && tset_in_set(k, TSET_SESSION_SCOPED_KEYS)
                 && strcmp(scope, "global") != 0;
    free(scope);
    if (stale) {
        json_t *na = tset_args0();
        tset_aset(na, "params", params);
        tset_aset(na, "rid", rid);
        json_t *nw = tset_call(ctx, "sess_nowait", na);
        if (tset_is_raise(nw)) return nw;
        /* return _sess_nowait(params, rid)[1] */
        if (nw && nw->type == JSON_ARRAY) {
            if (json_len(nw) < 2) {
                json_free(nw);
                return tset_err_spec("IndexError", "list index out of range");
            }
            json_t *r = json_copy(json_get(nw, 1));
            json_free(nw);
            return r;
        }
        char msg[160];
        if (!nw || nw->type == JSON_NULL)
            snprintf(msg, sizeof msg, "'NoneType' object is not subscriptable");
        else if (nw->type == JSON_STRING)
            snprintf(msg, sizeof msg, "string indices must be integers, not 'str'");
        else if (nw->type == JSON_OBJECT)
            snprintf(msg, sizeof msg, "dict indices must be integers or slices, not str");
        else
            snprintf(msg, sizeof msg, "'%s' object is not subscriptable", tset_type_name(nw));
        json_free(nw);
        return tset_err_spec("TypeError", msg);
    }
    /* handler dispatch */
    tset_handler_t handler = NULL;
    for (size_t i = 0; i < TSET_SETTER_COUNT; i++)
        if (!strcmp(TSET_SETTERS[i].key, k)) { handler = TSET_SETTERS[i].fn; break; }
    if (!handler && !strncmp(k, "details_mode.", 13)) handler = tset_set_details_section;
    else if (!handler && tset_in_set(k, TSET_DISPLAY_TOGGLE_KEYS)) handler = tset_set_display_toggle;
    if (!handler) {
        char msg[256];
        snprintf(msg, sizeof msg, "unknown config key: %s", k);
        return tset_err(ctx, rid, 4002, msg);
    }
    json_t *hargs = json_object();
    tset_aset(hargs, "rid", rid);
    tset_aset(hargs, "params", params);
    tset_aset(hargs, "key", key);
    tset_aset(hargs, "value", value);
    ctx->has_override = true;
    ctx->session_override = session;
    json_t *ret = handler(ctx, hargs);
    ctx->has_override = false;
    ctx->session_override = NULL;
    json_free(hargs);
    if (tset_is_raise(ret)) {
        const char *kind = tset_raise_kind(ret);
        if (kind && !strcmp(kind, "ManagedKeyError")) {
            char *msg = tset_strdup(json_get_str(ret, "message", ""));
            json_free(ret);
            json_t *e = tset_err(ctx, rid, 4030, msg);
            free(msg);
            return e;
        }
        if (kind && !strcmp(kind, "ConfigWriteError")) {
            char *msg = tset_strdup(json_get_str(ret, "message", ""));
            json_free(ret);
            json_t *e = tset_err(ctx, rid, 4002, msg);
            free(msg);
            return e;
        }
    }
    return ret;
}