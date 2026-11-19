/*
 * run_startup_flow2.c — FLOW port of the UNCLAIMED remainder of
 * gateway/run_startup.py (GatewayStartupMixin startup sequence / recovery /
 * handoff helpers). The claimed siblings live in run_startup_flow.c,
 * run_wrappers.c, run_gaps.c, run.c and run_shutdown_flow2.c; the EFFECTFUL
 * claimed siblings are SEAMED here (universal-seam calls) and the ported
 * siblings of THIS file compose REAL (internal composition).
 *
 * Ports the DECISION SEQUENCES of 24 functions:
 *   policy/env      — _own_policy_envs, _log_agent_budget,
 *                     _start_check_access_policy
 *   retry/queue     — _startup_retry_entry, _startup_queue_transient_failure,
 *                     _startup_teardown_adapter, _resume_pending_candidates
 *   ledger delivery — _release_runtime_claim_quiet, _obligation_adapter
 *   relay           — _on_relay_provisioned, _start_relay_provision_retry,
 *                     _start_register_plugins_relay_hooks
 *   hooks/config    — _register_config_hooks,
 *                     _register_launch_profile_config_hooks,
 *                     _recover_secondary_process_checkpoints
 *   boot logging    — _start_log_retired_session_reset,
 *                     _start_log_systemd_timing_alignment,
 *                     _open_faulthandler_log, _start_install_faulthandler,
 *                     _start_flush_runtime_status
 *   handoff         — _handoff_resolve_scope, _handoff_home_chat_type
 *   connect results — _start_aggregate_connect_results,
 *                     _start_secondary_profiles
 *
 * Modeling contract (shared with tests/oracle_run_startup_flow2.py):
 *  - Universal seam: json_t *(*call)(void *ud, const char *name, args). name
 *    identifies the live call; args is a BORROWED JSON object the stub copies
 *    into the trace; the result is a NEW json_t (NULL == None;
 *    {"__raise__":kind,"message":m} == a raised exception). The port owns args
 *    (rsu2_call frees them) and the result.
 *  - Seam names -> live calls (runner self is implicit, never projected):
 *      logger.debug/info/warning/error -> logger.<m>(fmt, *args, exc_info=?)
 *          args {fmt, args? (omitted when empty), exc_info? (true)}
 *      logger.log                   -> logger.log(level, fmt, *args, exc_info=?)
 *          args {fmt, level, args?, exc_info?}
 *      log_suppressed               -> _log_suppressed(level, msg, *args,
 *          exc_info=?) FACTORY call (claimed sibling CM). The CM semantics are
 *          TRANSCRIBED on both sides: body Exception -> logger.log(level, msg,
 *          *args[, exc-as-spec], exc_info=true); BaseException passes unlogged.
 *      os.getenv                    -> {name, default?} (default key only when
 *          the live call passes one)
 *      os.makedirs                  -> {path, exist_ok}
 *      open                         -> {path, mode, encoding}
 *      signal.SIGUSR2               -> {} (attribute-read seam; null == absent)
 *      faulthandler.enable          -> {} | {file, all_threads}
 *      faulthandler.register        -> {sigusr2, file, all_threads, chain}
 *      config.resolve_turn_limit    -> {value}
 *      config.load_config           -> {}
 *      platform_registry.get        -> {value}
 *      platform_registry.loaded_entries -> {}
 *      platform_env.authz_field     -> {value, field, entry}
 *      run._own_policy_open_startup_violation -> {config}
 *      run._write_runtime_status_quiet -> {gateway_state, exit_reason}
 *      run._profile_runtime_scope   -> {home, env?} (claimed CM factory;
 *          enter/exit abstracted — no observable effect beyond the factory)
 *      run.get_hermes_home          -> {}
 *      run._multiplex_profile_homes -> {}
 *      run.load_gateway_config      -> {}
 *      hermes_constants.get_hermes_home / get_process_hermes_home -> {}
 *      Path.resolve                 -> {path}
 *      self._request_clean_exit     -> {reason}
 *      self._reconnect_queue_entry  -> {platform, adapter, platform_config,
 *          attempts, delay, queued}
 *      self._update_platform_runtime_status -> {value, platform_state,
 *          error_code, error_message}
 *      self._safe_adapter_disconnect -> {adapter, platform}
 *      self._authorization_adapter  -> {platform, profile}
 *      self._retain_background_task -> {task}
 *      self._publish_primary_adapter -> {platform, adapter}
 *      self._startup_fail_fatal_config -> {reason} (claimed sibling)
 *      self._start_secondary_profile_adapters -> {}
 *      self._unserved_shared_ingress_warnings -> {}
 *      adapter.cancel_background_tasks -> {}
 *      adapter.get_chat_info        -> {home_chat_id}
 *      run_restored_delivery.resume_pending_candidates -> {platform}
 *      delivery_ledger.release_runtime_claim -> {obligation_id, error}
 *          (asyncio.to_thread is transparent: the inner call is the seam)
 *      delivery_ledger.is_flood_error -> {last_error}
 *      relay.relay_provision_pending / self_provision_relay /
 *          register_relay_adapter / relay_url / send_relay_policy -> {}
 *      relay.retry_self_provision   -> {callback}
 *      asyncio.create_task          -> {coro, name}
 *      plugins.sweep_pending_plugin_deletes / discover_plugins -> {}
 *      shell_hooks.register_from_config -> {config, accept_hooks}
 *      outbound_webhooks.register_from_config -> {config}
 *      secret_scope.is_multiplex_active -> {}
 *      profiles.profiles_to_serve   -> {multiplex}
 *      config_loader.read_yaml_layers -> {home}
 *      session_reset_retirement.retired_reset_policy -> {layers}
 *      session_reset_retirement.reset_plugin_enabled -> {}
 *      session_reset_retirement.format_notice -> {path, mode}
 *      shutdown_forensics.check_systemd_timing_alignment ->
 *          {drain_timeout, cron_drain_timeout}
 *      status.flush_runtime_status_async -> {timeout}
 *      process_registry.recover_from_checkpoint / restore_completions -> {}
 *      restart.is_global_startup_conflict -> {code}
 *  - Projections: Platform enum -> its .value string; attribute doubles ->
 *    field dicts (Platform dict keys -> value strings); exceptions ->
 *    {"__raise__":kind,"message":str(exc)}; bound ported methods passed as
 *    arguments -> {"__callable__":"<name>"}; Path doubles -> POSIX str.
 *    Explicit nulls are KEPT in seam args (no None-key dropping).
 *  - State mirror (in["state"], copied by the harness before mutation):
 *    config / adapters / _profile_adapters / _failed_platforms (objects keyed
 *    by platform .value), _startup_parked_platforms /
 *    _startup_ownership_conflict / _platform_lock_takeover_on_start (bools),
 *    startup_retryable_errors / startup_nonretryable_errors /
 *    multiplex_skipped_platforms (the CALLER'S mutable lists). Direct
 *    attribute reads on absent keys raise AttributeError "missing attribute
 *    'X'" (the oracle's AttrDouble message); getattr-with-default reads do not.
 *    _restart_drain_timeout / _cron_drain_timeout are NOT defaulted (absence
 *    is observable). Seamed siblings do NOT mutate the mirror.
 *  - Constants (byte-exact): TURN_LIMIT_UNLIMITED == sys.maxsize
 *    (9223372036854775807); DEFAULT_GATEWAY_CRON_DRAIN_TIMEOUT == 30.0;
 *    GATEWAY_FATAL_CONFIG_EXIT_CODE == 78 (unused here); the
 *    _ALLOWLIST_ENV_PLATFORMS / _BUILTIN_*_VARS tuples; the
 *    _HANDOFF_SHARED_CHAT_TYPES frozenset (membership only).
 *  - Exception kinds: `except Exception` / suppress(Exception) swallow the
 *    Exception family and pass the BaseException trio (SystemExit,
 *    KeyboardInterrupt, GeneratorExit) + CancelledError through. The FULL
 *    builtin hierarchy is tabulated below (rsu2_exc_parent), plus the custom
 *    MultiplexConfigError (Exception subclass). `except (RuntimeError,
 *    ValueError, OSError)` walks the same chain.
 *  - Scripted seam results: a missing/exhausted script is fail-closed — the
 *    harness dispatcher RETURNS a {"__raise__":"RuntimeError",...} spec and
 *    records it, so the port propagates it like any scripted raise.
 *  - FIXTURE DOMAIN (excluded divergences): json_t carries no int/float
 *    identity, so integral floats read as ints (fixtures keep str()-position
 *    numbers integral-int or non-integral, and avoid ints within 2^63 rounding
 *    distance of sys.maxsize except maxsize itself). Platform values are exact
 *    builtin lowercase members or clearly-unknown lowercase strings (the live
 *    _missing_ plugin pseudo-member/alias surface is out of domain). _raw
 *    connect items are Exception-family raise specs or 5-element lists with
 *    valid builtin platform values. Paths are POSIX (the oracle seeds
 *    PurePosixPath-based Path doubles and posixpath). Invalid int() tokens /
 *    repr'd strings are printable ASCII without quotes/backslashes.
 *    hasattr(faulthandler, "register") is the fixture flag
 *    in["faulthandler_has_register"] (default true) on BOTH sides (no event).
 *  - Self-contained: the harness #includes this .c; every symbol is static and
 *    rsu2_*-prefixed; only libjson is linked.
 *
 * PoP annotations (one per ported function) sit immediately before each def.
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
#include "libjson/json.h"

/* ── live module constants (byte-exact) ───────────────────────────────────── */
#define RSU2_TURN_LIMIT_UNLIMITED 9223372036854775807.0   /* sys.maxsize */
#define RSU2_DEFAULT_CRON_DRAIN_TIMEOUT 30.0
static const char *RSU2_PLATFORM_VALUES[] = {
    "local", "telegram", "discord", "whatsapp", "whatsapp_cloud", "slack",
    "signal", "mattermost", "matrix", "email", "sms", "dingtalk", "api_server",
    "webhook", "msgraph_webhook", "feishu", "wecom", "wecom_callback", "weixin",
    "bluebubbles", "qqbot", "relay",
};
/* _ALLOWLIST_ENV_PLATFORMS (order is part of the var tuples). */
static const char *RSU2_ALLOWLIST_ENV_PLATFORMS[] = {
    "TELEGRAM", "DISCORD", "WHATSAPP", "WHATSAPP_CLOUD", "SLACK", "SIGNAL",
    "EMAIL", "SMS", "MATTERMOST", "MATRIX", "DINGTALK", "FEISHU", "WECOM",
    "WECOM_CALLBACK", "WEIXIN", "BLUEBUBBLES", "QQ",
};
/* _HANDOFF_SHARED_CHAT_TYPES (membership only). */
static const char *RSU2_HANDOFF_SHARED_CHAT_TYPES[] = {
    "group", "channel", "forum", "room", "supergroup", "thread",
};

/* ── seam environment ─────────────────────────────────────────────────────── */
typedef struct rsu2_env {
    json_t *state;   /* borrowed mirror (harness-owned copy of in["state"]) */
    json_t *(*call)(void *ud, const char *name, const json_t *args);
    void *ud;
    bool has_register;  /* in["faulthandler_has_register"] (default true) */
} rsu2_env;

/* ── JSON type / truthiness helpers ───────────────────────────────────────── */
static bool rsu2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool rsu2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool rsu2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool rsu2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool rsu2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *rsu2_strval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static const char *rsu2_sval(const json_t *v) {
    const char *s = rsu2_strval(v); return s ? s : "";
}
static bool rsu2_truth(const json_t *v) {
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
/* present and not JSON null (models `x is not None`). */
static bool rsu2_nonnull(const json_t *v) { return v && v->type != JSON_NULL; }
static const char *rsu2_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 9.2e18) return "int";
            return "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* ── raise specs + FULL builtin exception hierarchy ───────────────────────── */
static bool rsu2_raised(const json_t *r) {
    return r && r->type == JSON_OBJECT && rsu2_is_str(json_obj_get(r, "__raise__"));
}
static const char *rsu2_kind(const json_t *r) { return json_get_str(r, "__raise__", ""); }
static const char *rsu2_msg(const json_t *r) { return json_get_str(r, "message", ""); }
static json_t *rsu2_raise(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "message", json_string(msg ? msg : ""));
    return r;
}
/* FULL builtin hierarchy (child -> parent) + MultiplexConfigError. */
static const struct { const char *c, *p; } RSU2_EXC_PARENT[] = {
    {"Exception","BaseException"},{"SystemExit","BaseException"},
    {"KeyboardInterrupt","BaseException"},{"GeneratorExit","BaseException"},
    {"CancelledError","BaseException"},{"BaseExceptionGroup","BaseException"},
    {"StopIteration","Exception"},{"StopAsyncIteration","Exception"},
    {"ArithmeticError","Exception"},{"FloatingPointError","ArithmeticError"},
    {"OverflowError","ArithmeticError"},{"ZeroDivisionError","ArithmeticError"},
    {"AssertionError","Exception"},{"AttributeError","Exception"},
    {"BufferError","Exception"},{"EOFError","Exception"},
    {"ImportError","Exception"},{"ModuleNotFoundError","ImportError"},
    {"LookupError","Exception"},{"IndexError","LookupError"},{"KeyError","LookupError"},
    {"MemoryError","Exception"},{"NameError","Exception"},{"UnboundLocalError","NameError"},
    {"OSError","Exception"},{"BlockingIOError","OSError"},{"ChildProcessError","OSError"},
    {"ConnectionError","OSError"},{"BrokenPipeError","ConnectionError"},
    {"ConnectionAbortedError","ConnectionError"},{"ConnectionRefusedError","ConnectionError"},
    {"ConnectionResetError","ConnectionError"},{"FileExistsError","OSError"},
    {"FileNotFoundError","OSError"},{"InterruptedError","OSError"},
    {"IsADirectoryError","OSError"},{"NotADirectoryError","OSError"},
    {"PermissionError","OSError"},{"ProcessLookupError","OSError"},{"TimeoutError","OSError"},
    {"IOError","OSError"},{"EnvironmentError","OSError"},{"WindowsError","OSError"},
    {"ReferenceError","Exception"},{"RuntimeError","Exception"},
    {"NotImplementedError","RuntimeError"},{"RecursionError","RuntimeError"},
    {"PythonFinalizationError","RuntimeError"},
    {"MultiplexConfigError","Exception"},
    {"SyntaxError","Exception"},{"IndentationError","SyntaxError"},{"TabError","IndentationError"},
    {"_IncompleteInputError","SyntaxError"},
    {"SystemError","Exception"},{"TypeError","Exception"},{"ValueError","Exception"},
    {"UnicodeError","ValueError"},{"UnicodeDecodeError","UnicodeError"},
    {"UnicodeEncodeError","UnicodeError"},{"UnicodeTranslateError","UnicodeError"},
    {"Warning","Exception"},{"UserWarning","Warning"},{"DeprecationWarning","Warning"},
    {"PendingDeprecationWarning","Warning"},{"SyntaxWarning","Warning"},
    {"RuntimeWarning","Warning"},{"FutureWarning","Warning"},{"ImportWarning","Warning"},
    {"UnicodeWarning","Warning"},{"BytesWarning","Warning"},{"ResourceWarning","Warning"},
    {"EncodingWarning","Warning"},{"ExceptionGroup","Exception"},
};
static const char *rsu2_exc_parent(const char *kind) {
    for (size_t i = 0; i < sizeof(RSU2_EXC_PARENT)/sizeof(RSU2_EXC_PARENT[0]); i++)
        if (strcmp(RSU2_EXC_PARENT[i].c, kind) == 0) return RSU2_EXC_PARENT[i].p;
    return NULL;
}
static bool rsu2_exc_is(const char *kind, const char *ancestor) {
    const char *k = kind;
    while (k) {
        if (strcmp(k, ancestor) == 0) return true;
        k = rsu2_exc_parent(k);
    }
    return false;
}
static bool rsu2_is_exception(const char *kind) { return rsu2_exc_is(kind, "Exception"); }
/* `except Exception` / suppress(Exception): swallow the Exception family (free
 * r), propagate BaseException. Returns true when the caller must propagate. */
static bool rsu2_swallow_exc(json_t *r) {
    if (!rsu2_is_exception(rsu2_kind(r))) return true;
    json_free(r);
    return false;
}

/* ── universal seam call (args owned by port, freed after the call) ───────── */
static json_t *rsu2_call(rsu2_env *e, const char *name, json_t *args) {
    json_t *r = e->call(e->ud, name, args);
    json_free(args);
    return r;
}
static json_t *rsu2_args(void) { return json_object(); }
static void rsu2_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void rsu2_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void rsu2_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void rsu2_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}

/* ── Python str()/repr() coercions (ASCII domain) ─────────────────────────── */
static void rsu2_float_repr(double v, char *out, size_t cap) {
    if (isnan(v)) { snprintf(out, cap, "nan"); return; }
    if (isinf(v)) { snprintf(out, cap, v < 0 ? "-inf" : "inf"); return; }
    if (v == 0.0) { snprintf(out, cap, signbit(v) ? "-0.0" : "0.0"); return; }
    bool neg = v < 0; double av = neg ? -v : v;
    char buf[64]; int p;
    for (p = 1; p <= 17; p++) {
        snprintf(buf, sizeof(buf), "%.*e", p - 1, av);
        if (strtod(buf, NULL) == av) break;
    }
    if (p > 17) p = 17;
    char digits[32]; size_t nd = 0;
    digits[nd++] = buf[0];
    for (int i = 2; buf[i] && buf[i] != 'e' && buf[i] != 'E'; i++) digits[nd++] = buf[i];
    digits[nd] = '\0';
    while (nd > 1 && digits[nd-1] == '0') digits[--nd] = '\0';
    int exp = atoi(strchr(buf, 'e') + 1);
    char body[128];
    if (exp >= -4 && exp < 16) {
        if (exp >= 0) {
            size_t ip = (size_t)exp + 1, o = 0;
            for (size_t i = 0; i < ip; i++) body[o++] = i < nd ? digits[i] : '0';
            body[o++] = '.';
            if (nd > ip) { for (size_t i = ip; i < nd; i++) body[o++] = digits[i]; }
            else body[o++] = '0';
            body[o] = '\0';
        } else {
            size_t o = 0; body[o++]='0'; body[o++]='.';
            for (int i = 0; i < -exp - 1; i++) body[o++] = '0';
            for (size_t i = 0; i < nd; i++) body[o++] = digits[i];
            body[o] = '\0';
        }
    } else {
        char frac[32]; size_t o = 0; frac[o++] = digits[0];
        if (nd > 1) { frac[o++] = '.'; for (size_t i = 1; i < nd; i++) frac[o++] = digits[i]; }
        frac[o] = '\0';
        snprintf(body, sizeof(body), "%se%c%02d", frac, exp < 0 ? '-' : '+', exp < 0 ? -exp : exp);
    }
    snprintf(out, cap, "%s%s", neg ? "-" : "", body);
}
static char *rsu2_py_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL: return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 9.2e18) {
                snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
                return strdup(buf);
            }
            rsu2_float_repr(v->num_val, buf, sizeof(buf));
            return strdup(buf);
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        default: return strdup("");
    }
}
static char *rsu2_py_repr(const char *s) {
    bool has_sq = strchr(s, '\'') != NULL, has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t cap = strlen(s) * 4 + 3;
    char *r = (char *)malloc(cap); size_t o = 0;
    r[o++] = q;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)q || c == '\\') { r[o++]='\\'; r[o++]=(char)c; }
        else if (c == '\n') { r[o++]='\\'; r[o++]='n'; }
        else if (c == '\r') { r[o++]='\\'; r[o++]='r'; }
        else if (c == '\t') { r[o++]='\\'; r[o++]='t'; }
        else if (c < 0x20 || c == 0x7F) { o += (size_t)snprintf(r+o, cap-o, "\\x%02x", c); }
        else r[o++] = (char)c;
    }
    r[o++] = q; r[o] = '\0';
    return r;
}
/* repr() of a JSON value for enum ValueError messages (str/int/float/None). */
static char *rsu2_value_repr(const json_t *v) {
    if (rsu2_is_str(v)) return rsu2_py_repr(rsu2_sval(v));
    return rsu2_py_str(v);
}

/* ── str.strip()/lower() (ASCII domain) ───────────────────────────────────── */
static bool rsu2_py_ws(char c) {
    return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f';
}
static char *rsu2_strip(const char *s) {
    if (!s) return strdup("");
    while (*s && rsu2_py_ws(*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && rsu2_py_ws(s[n-1])) n--;
    char *out = malloc(n+1); memcpy(out, s, n); out[n] = '\0';
    return out;
}
static char *rsu2_lower(const char *s) {
    char *out = strdup(s ? s : "");
    for (char *p = out; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    return out;
}

/* ── posixpath.join (two components; POSIX fixture domain) ────────────────── */
static char *rsu2_posix_join(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    if (b[0] == '/') return strdup(b);
    size_t na = strlen(a), nb = strlen(b);
    if (na == 0) return strdup(b);
    bool sep = a[na-1] == '/';
    char *out = malloc(na + nb + 2);
    strcpy(out, a);
    if (!sep) strcat(out, "/");
    strcat(out, b);
    return out;
}

/* ── attribute reads (AttrDouble semantics) ───────────────────────────────── */
static json_t *rsu2_attr_err(const json_t *recv, const char *key) {
    char buf[256];
    if (rsu2_is_obj(recv))
        snprintf(buf, sizeof(buf), "missing attribute '%s'", key);
    else
        snprintf(buf, sizeof(buf), "'%s' object has no attribute '%s'",
                 rsu2_typename(recv), key);
    return rsu2_raise("AttributeError", buf);
}
/* Direct attribute read: absent key (or non-object receiver) raises. Returns
 * a BORROWED value node, or NULL with *err set to an owned raise spec. */
static const json_t *rsu2_attr(const json_t *recv, const char *key, json_t **err) {
    *err = NULL;
    if (rsu2_is_obj(recv)) {
        const json_t *v = json_obj_get(recv, key);
        if (v) return v;
    }
    *err = rsu2_attr_err(recv, key);
    return NULL;
}
/* getattr(recv, key, default): never raises; returns borrowed value/default. */
static const json_t *rsu2_getattr(const json_t *recv, const char *key,
                                  const json_t *defval) {
    if (rsu2_is_obj(recv)) {
        const json_t *v = json_obj_get(recv, key);
        if (v) return v;
    }
    return defval;
}

/* ── logging seams (RecLogger twin) ───────────────────────────────────────── */
/* logger.<name>(fmt, *args, exc_info=?): {fmt, args?, exc_info?}. args_arr is
 * OWNED (consumed). Empty positional args omit the "args" key. */
static json_t *rsu2_log_named(rsu2_env *e, const char *name, const char *fmt,
                              json_t *args_arr, bool exc_info) {
    json_t *a = rsu2_args();
    rsu2_s(a, "fmt", fmt);
    if (args_arr && json_len(args_arr) > 0) json_set(a, "args", args_arr);
    else json_free(args_arr);
    if (exc_info) rsu2_b(a, "exc_info", true);
    return rsu2_call(e, name, a);
}
static json_t *rsu2_log(rsu2_env *e, const char *level, const char *fmt,
                        json_t *args_arr, bool exc_info) {
    char name[32];
    snprintf(name, sizeof(name), "logger.%s", level);
    return rsu2_log_named(e, name, fmt, args_arr, exc_info);
}
/* logger.log(level, fmt, *args, exc_info=?): {fmt, level, args?, exc_info?}. */
static json_t *rsu2_log_log(rsu2_env *e, int level, const char *fmt,
                            json_t *args_arr, bool exc_info) {
    json_t *a = rsu2_args();
    rsu2_s(a, "fmt", fmt);
    rsu2_n(a, "level", (double)level);
    if (args_arr && json_len(args_arr) > 0) json_set(a, "args", args_arr);
    else json_free(args_arr);
    if (exc_info) rsu2_b(a, "exc_info", true);
    return rsu2_call(e, "logger.log", a);
}
static json_t *rsu2_args1s(const char *s) {
    json_t *x = json_array();
    json_append(x, json_string(s ? s : ""));
    return x;
}

/* ── _log_suppressed CM (claimed sibling: factory seamed, semantics twin) ── */
/* Factory call; a scripted raise propagates at the with-statement. */
static json_t *rsu2_ls_enter(rsu2_env *e, int level, const char *fmt,
                             const json_t *args_arr, bool exc_info) {
    json_t *a = rsu2_args();
    rsu2_n(a, "level", (double)level);
    rsu2_s(a, "fmt", fmt);
    if (args_arr && json_len(args_arr) > 0) json_set(a, "args", json_copy(args_arr));
    if (exc_info) rsu2_b(a, "exc_info", true);
    return rsu2_call(e, "log_suppressed", a);
}
/* CM exit: body_raise owned (NULL == no raise). Exception family -> transcribed
 * logger.log + swallow; BaseException -> propagate. Returns NULL (swallowed /
 * clean) or an owned raise spec to propagate. */
static json_t *rsu2_ls_exit(rsu2_env *e, int level, const char *fmt,
                            const json_t *args_arr, bool exc_info,
                            json_t *body_raise) {
    if (!body_raise) return NULL;
    if (!rsu2_raised(body_raise)) { json_free(body_raise); return NULL; }
    if (!rsu2_is_exception(rsu2_kind(body_raise))) return body_raise;
    json_t *la = args_arr ? json_copy(args_arr) : json_array();
    if (!exc_info) json_append(la, json_copy(body_raise));  /* exc appended as last arg */
    json_t *lr = rsu2_log_log(e, level, fmt, la, exc_info);
    json_free(body_raise);
    return lr;   /* NULL or a logger raise (propagates) */
}

/* ── numeric add with live TypeError (int += seam result) ─────────────────── */
static json_t *rsu2_iadd(double acc, const json_t *r, double *out) {
    if (!rsu2_is_num(r)) {
        char buf[192];
        snprintf(buf, sizeof(buf),
                 "unsupported operand type(s) for +=: 'int' and '%s'",
                 rsu2_typename(r));
        return rsu2_raise("TypeError", buf);
    }
    *out = acc + r->num_val;
    return NULL;
}

/* ── Platform(value) ctor (builtin members; ValueError otherwise) ─────────── */
static json_t *rsu2_platform_ctor(const json_t *v, const char **out) {
    if (rsu2_is_str(v)) {
        const char *s = rsu2_sval(v);
        for (size_t i = 0; i < sizeof(RSU2_PLATFORM_VALUES)/sizeof(char*); i++)
            if (strcmp(s, RSU2_PLATFORM_VALUES[i]) == 0) { *out = s; return NULL; }
    }
    char *rep = rsu2_value_repr(v);
    char buf[512];
    snprintf(buf, sizeof(buf), "%s is not a valid Platform", rep);
    free(rep);
    return rsu2_raise("ValueError", buf);
}

/* ── state mirror access ──────────────────────────────────────────────────── */
static json_t *rsu2_st_obj(rsu2_env *e, const char *key) {
    json_t *v = json_obj_get(e->state, key);
    return rsu2_is_obj(v) ? v : NULL;
}
static json_t *rsu2_st_arr(rsu2_env *e, const char *key) {
    json_t *v = json_obj_get(e->state, key);
    return rsu2_is_arr(v) ? v : NULL;
}

/* ── forward declarations (ported siblings compose REAL) ──────────────────── */
static json_t *rsu2_own_policy_envs(rsu2_env *e, const json_t *platform_value);
static json_t *rsu2_startup_retry_entry(rsu2_env *e, const char *pvalue,
                                        const json_t *adapter,
                                        const json_t *platform_config, bool queued);
static json_t *rsu2_release_runtime_claim_quiet(rsu2_env *e, const json_t *obligation_id,
                                                const char *log_fmt, const json_t *error);
static json_t *rsu2_register_config_hooks(rsu2_env *e, const char *fail_fmt,
                                          const json_t *fail_args, int level);
static json_t *rsu2_register_launch_profile_config_hooks(rsu2_env *e);
static json_t *rsu2_open_faulthandler_log(rsu2_env *e);
/* ── 1. _own_policy_envs (module function) ────────────────────────────────── */
/* PoP: rsu2_own_policy_envs @ gateway/run_startup.py:_own_policy_envs */
static json_t *rsu2_own_policy_envs(rsu2_env *e, const json_t *platform_value) {
    const char *pv = rsu2_sval(platform_value);
    json_t *a1 = rsu2_args();
    rsu2_s(a1, "value", pv);
    json_t *entry = rsu2_call(e, "platform_registry.get", a1);
    if (rsu2_raised(entry)) return entry;
    json_t *a2 = rsu2_args();
    rsu2_s(a2, "value", pv);
    rsu2_s(a2, "field", "own_policy_env");
    rsu2_j(a2, "entry", entry);
    json_t *own = rsu2_call(e, "platform_env.authz_field", a2);
    if (rsu2_raised(own)) { json_free(entry); return own; }
    if (!rsu2_truth(own)) { json_free(own); json_free(entry); return NULL; }
    if (!rsu2_is_arr(own)) {
        /* (*own, x) splat: Value after * must be an iterable */
        char buf[192];
        snprintf(buf, sizeof(buf), "Value after * must be an iterable, not %s",
                 rsu2_typename(own));
        json_free(own); json_free(entry);
        return rsu2_raise("TypeError", buf);
    }
    json_t *a3 = rsu2_args();
    rsu2_s(a3, "value", pv);
    rsu2_s(a3, "field", "allow_all_env");
    rsu2_j(a3, "entry", entry);
    json_t *allow_all = rsu2_call(e, "platform_env.authz_field", a3);
    if (rsu2_raised(allow_all)) { json_free(own); json_free(entry); return allow_all; }
    json_t *out = json_array();
    for (size_t i = 0; i < json_len(own); i++) json_append(out, json_copy(json_get(own, i)));
    json_append(out, allow_all ? allow_all : json_null());
    json_free(own); json_free(entry);
    return out;
}

/* ── 2. _log_agent_budget (staticmethod) ──────────────────────────────────── */
/* PoP: rsu2_log_agent_budget @ gateway/run_startup.py:_log_agent_budget */
static json_t *rsu2_log_agent_budget(rsu2_env *e) {
    json_t *a1 = rsu2_args();
    rsu2_s(a1, "name", "HERMES_MAX_ITERATIONS");
    json_t *raw = rsu2_call(e, "os.getenv", a1);
    if (rsu2_raised(raw)) return raw;
    json_t *a2 = rsu2_args();
    rsu2_j(a2, "value", raw);
    json_free(raw);
    json_t *limit = rsu2_call(e, "config.resolve_turn_limit", a2);
    if (rsu2_raised(limit)) return limit;
    /* limit == TURN_LIMIT_UNLIMITED (sys.maxsize) */
    bool unlimited = rsu2_is_num(limit) && limit->num_val == RSU2_TURN_LIMIT_UNLIMITED;
    json_t *args = json_array();
    if (unlimited) json_append(args, json_string("unlimited"));
    else json_append(args, limit ? json_copy(limit) : json_null());
    json_free(limit);
    json_t *lr = rsu2_log(e, "info",
        "Agent budget: max_iterations=%s (agent.max_turns from config.yaml, "
        "else the HERMES_MAX_ITERATIONS bridge)", args, false);
    if (rsu2_raised(lr)) return lr;
    json_free(lr);
    return NULL;
}

/* ── 3. _startup_retry_entry ──────────────────────────────────────────────── */
/* PoP: rsu2_startup_retry_entry @ gateway/run_startup.py:_startup_retry_entry */
static json_t *rsu2_startup_retry_entry(rsu2_env *e, const char *pvalue,
                                        const json_t *adapter,
                                        const json_t *platform_config, bool queued) {
    json_t *a = rsu2_args();
    rsu2_s(a, "platform", pvalue);
    rsu2_j(a, "adapter", adapter);
    rsu2_j(a, "platform_config", platform_config);
    rsu2_n(a, "attempts", 1);
    rsu2_n(a, "delay", 30);
    rsu2_b(a, "queued", queued);
    return rsu2_call(e, "self._reconnect_queue_entry", a);
}

/* ── 4. _startup_queue_transient_failure ──────────────────────────────────── */
/* PoP: rsu2_startup_queue_transient_failure @ gateway/run_startup.py:_startup_queue_transient_failure */
static json_t *rsu2_startup_queue_transient_failure(rsu2_env *e, const char *pvalue,
                                                    const json_t *adapter,
                                                    const json_t *platform_config,
                                                    const char *message) {
    json_t *a = rsu2_args();
    rsu2_s(a, "value", pvalue);
    rsu2_s(a, "platform_state", "retrying");
    rsu2_s(a, "error_code", NULL);
    rsu2_s(a, "error_message", message);
    json_t *r = rsu2_call(e, "self._update_platform_runtime_status", a);
    if (rsu2_raised(r)) return r;
    json_free(r);
    /* startup_retryable_errors.append(f"{platform.value}: {message}") */
    json_t *errs = rsu2_st_arr(e, "startup_retryable_errors");
    if (!errs) { errs = json_array(); json_set(e->state, "startup_retryable_errors", errs); }
    size_t n = strlen(pvalue) + strlen(message ? message : "") + 3;
    char *line = malloc(n);
    snprintf(line, n, "%s: %s", pvalue, message ? message : "");
    json_append(errs, json_string(line));
    free(line);
    /* self._failed_platforms[platform] = self._startup_retry_entry(...) */
    json_t *entry = rsu2_startup_retry_entry(e, pvalue, adapter, platform_config, true);
    if (rsu2_raised(entry)) return entry;
    json_t *fp = rsu2_st_obj(e, "_failed_platforms");
    if (!fp) { fp = json_object(); json_set(e->state, "_failed_platforms", fp); }
    json_set(fp, pvalue, entry ? entry : json_null());
    return NULL;
}

/* ── 5. _startup_teardown_adapter (async) ─────────────────────────────────── */
/* PoP: rsu2_startup_teardown_adapter @ gateway/run_startup.py:_startup_teardown_adapter */
static json_t *rsu2_startup_teardown_adapter(rsu2_env *e, const json_t *adapter,
                                             const char *pvalue) {
    json_t *fmt_args = rsu2_args1s(pvalue);
    json_t *ent = rsu2_ls_enter(e, 10,
        "\xe2\x9c\x97 %s background-task cancel error: %s", fmt_args, false);
    if (rsu2_raised(ent)) { json_free(fmt_args); return ent; }
    json_free(ent);
    json_t *body = rsu2_call(e, "adapter.cancel_background_tasks", rsu2_args());
    json_t *ex = rsu2_ls_exit(e, 10,
        "\xe2\x9c\x97 %s background-task cancel error: %s", fmt_args, false, body);
    json_free(fmt_args);
    if (ex) return ex;
    /* await self._safe_adapter_disconnect(adapter, platform) — OUTSIDE the CM */
    json_t *a = rsu2_args();
    rsu2_j(a, "adapter", adapter);
    rsu2_s(a, "platform", pvalue);
    return rsu2_call(e, "self._safe_adapter_disconnect", a);
}

/* ── 6. _resume_pending_candidates ────────────────────────────────────────── */
/* PoP: rsu2_resume_pending_candidates @ gateway/run_startup.py:_resume_pending_candidates */
static json_t *rsu2_resume_pending_candidates(rsu2_env *e, const json_t *platform) {
    json_t *a = rsu2_args();
    rsu2_j(a, "platform", platform);
    return rsu2_call(e, "run_restored_delivery.resume_pending_candidates", a);
}

/* ── 7. _release_runtime_claim_quiet (staticmethod async) ─────────────────── */
/* PoP: rsu2_release_runtime_claim_quiet @ gateway/run_startup.py:_release_runtime_claim_quiet */
static json_t *rsu2_release_runtime_claim_quiet(rsu2_env *e, const json_t *obligation_id,
                                                const char *log_fmt, const json_t *error) {
    json_t *a = rsu2_args();
    rsu2_j(a, "obligation_id", obligation_id);
    rsu2_j(a, "error", error);
    json_t *r = rsu2_call(e, "delivery_ledger.release_runtime_claim", a);
    if (rsu2_raised(r)) {
        if (rsu2_swallow_exc(r)) return r;   /* BaseException passes */
        json_t *la = json_array();
        json_append(la, obligation_id ? json_copy(obligation_id) : json_null());
        json_t *lr = rsu2_log(e, "debug", log_fmt, la, true);
        if (rsu2_raised(lr)) return lr;
        json_free(lr);
    } else {
        json_free(r);
    }
    return NULL;
}

/* ── 8. _obligation_adapter (async) ───────────────────────────────────────── */
/* PoP: rsu2_obligation_adapter @ gateway/run_startup.py:_obligation_adapter */
static json_t *rsu2_obligation_adapter(rsu2_env *e, const json_t *row) {
    /* try: platform = Platform(row["platform"]) */
    const json_t *pv = rsu2_is_obj(row) ? json_obj_get(row, "platform") : NULL;
    json_t *ctor_err = NULL;
    const char *pvalue = NULL;
    if (!pv) {
        ctor_err = rsu2_raise("KeyError", "'platform'");
    } else {
        ctor_err = rsu2_platform_ctor(pv, &pvalue);
    }
    if (ctor_err) {
        if (rsu2_swallow_exc(ctor_err)) return ctor_err;
        /* logger.debug("obligation %s: unknown platform %r",
         *              row["obligation_id"], row.get("platform")) */
        const json_t *oid = rsu2_is_obj(row) ? json_obj_get(row, "obligation_id") : NULL;
        if (!oid) return rsu2_raise("KeyError", "'obligation_id'");
        json_t *la = json_array();
        json_append(la, json_copy(oid));
        json_append(la, pv ? json_copy(pv) : json_null());
        json_t *lr = rsu2_log(e, "debug", "obligation %s: unknown platform %r", la, false);
        if (rsu2_raised(lr)) return lr;
        json_free(lr);
        return NULL;
    }
    const json_t *adapter = NULL;
    if (rsu2_is_obj(row) && json_has(row, "profile")) {
        json_t *a = rsu2_args();
        rsu2_s(a, "platform", pvalue);
        rsu2_j(a, "profile", json_obj_get(row, "profile"));
        json_t *r = rsu2_call(e, "self._authorization_adapter", a);
        if (rsu2_raised(r)) return r;
        adapter = r;   /* owned from here */
    } else {
        json_t *adapters = rsu2_st_obj(e, "adapters");
        const json_t *v = adapters ? json_obj_get(adapters, pvalue) : NULL;
        adapter = v ? json_copy(v) : NULL;
    }
    if (!rsu2_nonnull(adapter)) {
        /* from gateway.delivery_ledger import is_flood_error */
        const json_t *last_error = rsu2_is_obj(row) ? json_obj_get(row, "last_error") : NULL;
        json_t *a = rsu2_args();
        rsu2_j(a, "last_error", last_error);
        json_t *flood = rsu2_call(e, "delivery_ledger.is_flood_error", a);
        if (rsu2_raised(flood)) { json_free((json_t *)adapter); return flood; }
        bool is_flood = rsu2_truth(flood);
        json_free(flood);
        const json_t *oid = json_obj_get(row, "obligation_id");
        if (!oid) { json_free((json_t *)adapter); return rsu2_raise("KeyError", "'obligation_id'"); }
        /* error=last_error if is_flood_error(last_error) else "send_path_degraded" */
        json_t *err_node = is_flood ? (last_error ? json_copy(last_error) : json_null())
                                    : json_string("send_path_degraded");
        json_t *r = rsu2_release_runtime_claim_quiet(e, oid,
            "failed to release undispatched runtime obligation %s", err_node);
        json_free(err_node);
        if (rsu2_raised(r)) { json_free((json_t *)adapter); return r; }
        json_free(r);
    }
    return (json_t *)adapter;   /* NULL == None */
}

/* ── 9. _on_relay_provisioned ─────────────────────────────────────────────── */
/* PoP: rsu2_on_relay_provisioned @ gateway/run_startup.py:_on_relay_provisioned */
static json_t *rsu2_on_relay_provisioned(rsu2_env *e) {
    /* info = getattr(self, "_failed_platforms", {}).get(Platform.RELAY) */
    const json_t *fp = rsu2_getattr(e->state, "_failed_platforms", NULL);
    const json_t *info = NULL;
    if (rsu2_is_obj(fp)) {
        info = json_obj_get(fp, "relay");
    } else if (rsu2_nonnull(fp)) {
        char buf[192];
        snprintf(buf, sizeof(buf), "'%s' object has no attribute 'get'", rsu2_typename(fp));
        return rsu2_raise("AttributeError", buf);
    } else {
        return rsu2_raise("AttributeError", "'NoneType' object has no attribute 'get'");
    }
    if (rsu2_nonnull(info)) {
        if (!rsu2_is_obj(info)) {
            char buf[192];
            snprintf(buf, sizeof(buf), "'%s' object does not support item assignment",
                     rsu2_typename(info));
            return rsu2_raise("TypeError", buf);
        }
        json_set((json_t *)info, "next_retry", json_number(0.0));
        json_set((json_t *)info, "retry_now", json_bool(true));
    }
    return NULL;
}

/* ── 10. _start_relay_provision_retry ─────────────────────────────────────── */
/* except Exception: logger.warning("relay self-provision retry could not
 * start", exc_info=True). Returns NULL (swallowed) or an owned raise. */
static json_t *rsu2_relay_retry_except(rsu2_env *e, json_t *r) {
    if (rsu2_swallow_exc(r)) return r;
    json_t *lr = rsu2_log(e, "warning", "relay self-provision retry could not start",
                          NULL, true);
    if (rsu2_raised(lr)) return lr;
    json_free(lr);
    return NULL;
}
/* PoP: rsu2_start_relay_provision_retry @ gateway/run_startup.py:_start_relay_provision_retry */
static json_t *rsu2_start_relay_provision_retry(rsu2_env *e) {
    json_t *pending = rsu2_call(e, "relay.relay_provision_pending", rsu2_args());
    if (rsu2_raised(pending)) return rsu2_relay_retry_except(e, pending);
    bool go = rsu2_truth(pending);
    json_free(pending);
    if (!go) return NULL;
    json_t *cb = json_object();
    json_set(cb, "__callable__", json_string("_on_relay_provisioned"));
    json_t *a1 = rsu2_args();
    json_set(a1, "callback", cb);
    json_t *coro = rsu2_call(e, "relay.retry_self_provision", a1);
    if (rsu2_raised(coro)) return rsu2_relay_retry_except(e, coro);
    json_t *a2 = rsu2_args();
    rsu2_j(a2, "coro", coro);
    json_free(coro);
    rsu2_s(a2, "name", "relay-provision-retry");
    json_t *task = rsu2_call(e, "asyncio.create_task", a2);
    if (rsu2_raised(task)) return rsu2_relay_retry_except(e, task);
    json_t *a3 = rsu2_args();
    rsu2_j(a3, "task", task);
    json_free(task);
    json_t *r = rsu2_call(e, "self._retain_background_task", a3);
    if (rsu2_raised(r)) return rsu2_relay_retry_except(e, r);
    json_free(r);
    return NULL;
}

/* ── 11. _register_config_hooks (staticmethod) ────────────────────────────── */
/* PoP: rsu2_register_config_hooks @ gateway/run_startup.py:_register_config_hooks */
static json_t *rsu2_register_config_hooks(rsu2_env *e, const char *fail_fmt,
                                          const json_t *fail_args, int level) {
    json_t *cfg = rsu2_call(e, "config.load_config", rsu2_args());
    json_t *body = NULL;
    if (rsu2_raised(cfg)) {
        body = cfg;
    } else {
        json_t *a1 = rsu2_args();
        rsu2_j(a1, "config", cfg);
        rsu2_b(a1, "accept_hooks", false);
        json_t *r1 = rsu2_call(e, "shell_hooks.register_from_config", a1);
        if (rsu2_raised(r1)) {
            body = r1;
        } else {
            json_free(r1);
            json_t *a2 = rsu2_args();
            rsu2_j(a2, "config", cfg);
            json_t *r2 = rsu2_call(e, "outbound_webhooks.register_from_config", a2);
            if (rsu2_raised(r2)) body = r2;
            else json_free(r2);
        }
        json_free(cfg);
    }
    if (body) {
        if (rsu2_swallow_exc(body)) return body;
        json_t *la = fail_args ? json_copy(fail_args) : json_array();
        json_t *lr = rsu2_log_log(e, level, fail_fmt, la, true);
        if (rsu2_raised(lr)) return lr;
        json_free(lr);
    }
    return NULL;
}

/* ── 12. _register_launch_profile_config_hooks (staticmethod) ─────────────── */
/* PoP: rsu2_register_launch_profile_config_hooks @ gateway/run_startup.py:_register_launch_profile_config_hooks */
static json_t *rsu2_register_launch_profile_config_hooks(rsu2_env *e) {
    static const char *FAIL_FMT =
        "shell-hook/webhook registration failed at gateway startup";
    json_t *mp = rsu2_call(e, "secret_scope.is_multiplex_active", rsu2_args());
    if (rsu2_raised(mp)) return mp;
    bool multiplex = rsu2_truth(mp);
    json_free(mp);
    if (!multiplex)
        return rsu2_register_config_hooks(e, FAIL_FMT, NULL, 30);
    json_t *home = rsu2_call(e, "hermes_constants.get_process_hermes_home", rsu2_args());
    if (rsu2_raised(home)) return home;
    json_t *a = rsu2_args();
    rsu2_j(a, "home", home);
    json_free(home);
    json_t *scope = rsu2_call(e, "run._profile_runtime_scope", a);
    if (rsu2_raised(scope)) return scope;
    json_free(scope);
    return rsu2_register_config_hooks(e, FAIL_FMT, NULL, 30);
}

/* ── 13. _start_register_plugins_relay_hooks (staticmethod) ───────────────── */
/* except Exception: logger.warning("relay adapter registration failed at
 * gateway startup", exc_info=True). */
static json_t *rsu2_relay_reg_except(rsu2_env *e, json_t *r) {
    if (rsu2_swallow_exc(r)) return r;
    json_t *lr = rsu2_log(e, "warning",
                          "relay adapter registration failed at gateway startup",
                          NULL, true);
    if (rsu2_raised(lr)) return lr;
    json_free(lr);
    return NULL;
}
/* PoP: rsu2_start_register_plugins_relay_hooks @ gateway/run_startup.py:_start_register_plugins_relay_hooks */
static json_t *rsu2_start_register_plugins_relay_hooks(rsu2_env *e) {
    static const char *DISC_FMT = "plugin discovery failed at gateway startup";
    json_t *ent = rsu2_ls_enter(e, 30, DISC_FMT, NULL, true);
    if (rsu2_raised(ent)) return ent;
    json_free(ent);
    json_t *body = rsu2_call(e, "plugins.sweep_pending_plugin_deletes", rsu2_args());
    if (!rsu2_raised(body)) {
        json_free(body);
        body = rsu2_call(e, "plugins.discover_plugins", rsu2_args());
    }
    json_t *ex = rsu2_ls_exit(e, 30, DISC_FMT, NULL, true, body);
    if (ex) return ex;
    /* relay try block */
    json_t *sp = rsu2_call(e, "relay.self_provision_relay", rsu2_args());
    if (rsu2_raised(sp)) {
        json_t *r = rsu2_relay_reg_except(e, sp);
        if (r) return r;
    } else {
        json_free(sp);
        json_t *reg = rsu2_call(e, "relay.register_relay_adapter", rsu2_args());
        if (rsu2_raised(reg)) {
            json_t *r = rsu2_relay_reg_except(e, reg);
            if (r) return r;
        } else if (rsu2_truth(reg)) {
            json_free(reg);
            json_t *url = rsu2_call(e, "relay.relay_url", rsu2_args());
            if (rsu2_raised(url)) {
                json_t *r = rsu2_relay_reg_except(e, url);
                if (r) return r;
            } else {
                json_t *la = json_array();
                json_append(la, url ? url : json_null());
                json_t *lr = rsu2_log(e, "info",
                    "relay adapter registered (connector at %s)", la, false);
                if (rsu2_raised(lr)) {
                    json_t *r = rsu2_relay_reg_except(e, lr);
                    if (r) return r;
                } else {
                    json_free(lr);
                    json_t *pol = rsu2_call(e, "relay.send_relay_policy", rsu2_args());
                    if (rsu2_raised(pol)) {
                        json_t *r = rsu2_relay_reg_except(e, pol);
                        if (r) return r;
                    } else {
                        json_free(pol);
                    }
                }
            }
        } else {
            json_free(reg);
        }
    }
    /* claimed-sibling composition OUTSIDE the relay try */
    return rsu2_register_launch_profile_config_hooks(e);
}

/* ── 14. _recover_secondary_process_checkpoints ───────────────────────────── */
/* PoP: rsu2_recover_secondary_process_checkpoints @ gateway/run_startup.py:_recover_secondary_process_checkpoints */
static json_t *rsu2_recover_secondary_process_checkpoints(rsu2_env *e) {
    json_t *err = NULL;
    const json_t *config = rsu2_attr(e->state, "config", &err);
    if (err) return err;
    const json_t *mp = rsu2_getattr(config, "multiplex_profiles", NULL);
    if (!rsu2_truth(mp)) return json_number(0);
    json_t *hh = rsu2_call(e, "hermes_constants.get_hermes_home", rsu2_args());
    if (rsu2_raised(hh)) return hh;
    json_t *a0 = rsu2_args();
    rsu2_j(a0, "path", hh);
    json_free(hh);
    json_t *launch = rsu2_call(e, "Path.resolve", a0);
    if (rsu2_raised(launch)) return launch;
    json_t *homes = rsu2_call(e, "run._multiplex_profile_homes", rsu2_args());
    if (rsu2_raised(homes)) { json_free(launch); return homes; }
    if (!rsu2_is_arr(homes)) {
        char buf[192];
        snprintf(buf, sizeof(buf), "'%s' object is not iterable", rsu2_typename(homes));
        json_free(homes); json_free(launch);
        return rsu2_raise("TypeError", buf);
    }
    double recovered = 0;
    json_t *out_raise = NULL;
    for (size_t i = 0; i < json_len(homes) && !out_raise; i++) {
        const json_t *pair = json_get(homes, i);
        if (!rsu2_is_arr(pair)) {
            char buf[192];
            snprintf(buf, sizeof(buf), "cannot unpack non-iterable %s object",
                     rsu2_typename(pair));
            out_raise = rsu2_raise("TypeError", buf);
            break;
        }
        if (json_len(pair) != 2) {
            char buf[192];
            if (json_len(pair) < 2)
                snprintf(buf, sizeof(buf),
                         "not enough values to unpack (expected 2, got %zu)", json_len(pair));
            else
                snprintf(buf, sizeof(buf),
                         "too many values to unpack (expected 2, got %zu)", json_len(pair));
            out_raise = rsu2_raise("ValueError", buf);
            break;
        }
        const json_t *name = json_get(pair, 0);
        const json_t *home = json_get(pair, 1);
        /* Path(profile_home).resolve() == launch_home — OUTSIDE the try */
        json_t *a1 = rsu2_args();
        rsu2_j(a1, "path", home);
        json_t *res = rsu2_call(e, "Path.resolve", a1);
        if (rsu2_raised(res)) { out_raise = res; break; }
        bool same = strcmp(rsu2_sval(res), rsu2_sval(launch)) == 0;
        json_free(res);
        if (same) continue;
        /* try: with _profile_runtime_scope(Path(profile_home), {}): ... */
        json_t *a2 = rsu2_args();
        rsu2_j(a2, "home", home);
        json_set(a2, "env", json_object());
        json_t *scope = rsu2_call(e, "run._profile_runtime_scope", a2);
        json_t *body = NULL;
        if (rsu2_raised(scope)) {
            body = scope;
        } else {
            json_free(scope);
            json_t *rc = rsu2_call(e, "process_registry.recover_from_checkpoint", rsu2_args());
            if (rsu2_raised(rc)) {
                body = rc;
            } else {
                double sum = 0;
                json_t *ae = rsu2_iadd(recovered, rc, &sum);
                if (ae) body = ae;
                else recovered = sum;
                json_free(rc);
            }
        }
        if (body) {
            if (rsu2_swallow_exc(body)) { out_raise = body; break; }
            json_t *la = json_array();
            json_append(la, name ? json_copy(name) : json_null());
            json_t *lr = rsu2_log(e, "warning",
                "Process checkpoint recovery for profile %r failed", la, true);
            if (rsu2_raised(lr)) { out_raise = lr; break; }
            json_free(lr);
        }
    }
    json_free(homes);
    json_free(launch);
    if (out_raise) return out_raise;
    return json_number(recovered);
}

/* ── 15. _start_log_retired_session_reset ─────────────────────────────────── */
/* PoP: rsu2_start_log_retired_session_reset @ gateway/run_startup.py:_start_log_retired_session_reset */
static json_t *rsu2_start_log_retired_session_reset(rsu2_env *e) {
    static const char *FMT = "retired session_reset check failed";
    json_t *ent = rsu2_ls_enter(e, 10, FMT, NULL, true);
    if (rsu2_raised(ent)) return ent;
    json_free(ent);
    json_t *body = NULL;
    json_t *err = NULL;
    const json_t *config = rsu2_attr(e->state, "config", &err);
    if (err) {
        body = err;
    } else {
        const json_t *mp = rsu2_attr(config, "multiplex_profiles", &err);
        if (err) {
            body = err;
        } else {
            json_t *a = rsu2_args();
            rsu2_b(a, "multiplex", rsu2_truth(mp));
            json_t *profiles = rsu2_call(e, "profiles.profiles_to_serve", a);
            if (rsu2_raised(profiles)) {
                body = profiles;
            } else if (!rsu2_is_arr(profiles)) {
                char buf[192];
                snprintf(buf, sizeof(buf), "'%s' object is not iterable",
                         rsu2_typename(profiles));
                json_free(profiles);
                body = rsu2_raise("TypeError", buf);
            } else {
                json_t *hits = json_array();
                for (size_t i = 0; i < json_len(profiles) && !body; i++) {
                    const json_t *pair = json_get(profiles, i);
                    if (!rsu2_is_arr(pair)) {
                        char buf[192];
                        snprintf(buf, sizeof(buf), "cannot unpack non-iterable %s object",
                                 rsu2_typename(pair));
                        body = rsu2_raise("TypeError", buf);
                        break;
                    }
                    if (json_len(pair) != 2) {
                        char buf[192];
                        if (json_len(pair) < 2)
                            snprintf(buf, sizeof(buf),
                                     "not enough values to unpack (expected 2, got %zu)",
                                     json_len(pair));
                        else
                            snprintf(buf, sizeof(buf),
                                     "too many values to unpack (expected 2, got %zu)",
                                     json_len(pair));
                        body = rsu2_raise("ValueError", buf);
                        break;
                    }
                    const json_t *name = json_get(pair, 0);
                    const json_t *home = json_get(pair, 1);
                    json_t *a1 = rsu2_args();
                    rsu2_j(a1, "home", home);
                    json_t *layers = rsu2_call(e, "config_loader.read_yaml_layers", a1);
                    if (rsu2_raised(layers)) { body = layers; break; }
                    json_t *a2 = rsu2_args();
                    rsu2_j(a2, "layers", layers);
                    json_free(layers);
                    json_t *found = rsu2_call(e,
                        "session_reset_retirement.retired_reset_policy", a2);
                    if (rsu2_raised(found)) { body = found; break; }
                    if (rsu2_truth(found)) {
                        json_t *hit = json_array();
                        json_append(hit, name ? json_copy(name) : json_null());
                        json_append(hit, json_copy(found));
                        json_append(hits, hit);
                    }
                    json_free(found);
                }
                if (!body && json_len(hits) > 0) {
                    json_t *rpe = rsu2_call(e,
                        "session_reset_retirement.reset_plugin_enabled", rsu2_args());
                    if (rsu2_raised(rpe)) {
                        body = rpe;
                    } else if (!rsu2_truth(rpe)) {
                        json_free(rpe);
                        for (size_t i = 0; i < json_len(hits) && !body; i++) {
                            const json_t *hit = json_get(hits, i);
                            const json_t *name = json_get(hit, 0);
                            const json_t *found = json_get(hit, 1);
                            if (!rsu2_is_arr(found)) {
                                char buf[192];
                                snprintf(buf, sizeof(buf),
                                         "cannot unpack non-iterable %s object",
                                         rsu2_typename(found));
                                body = rsu2_raise("TypeError", buf);
                                break;
                            }
                            if (json_len(found) != 2) {
                                char buf[192];
                                if (json_len(found) < 2)
                                    snprintf(buf, sizeof(buf),
                                             "not enough values to unpack (expected 2, got %zu)",
                                             json_len(found));
                                else
                                    snprintf(buf, sizeof(buf),
                                             "too many values to unpack (expected 2, got %zu)",
                                             json_len(found));
                                body = rsu2_raise("ValueError", buf);
                                break;
                            }
                            json_t *a3 = rsu2_args();
                            rsu2_j(a3, "path", json_get(found, 0));
                            rsu2_j(a3, "mode", json_get(found, 1));
                            json_t *notice = rsu2_call(e,
                                "session_reset_retirement.format_notice", a3);
                            if (rsu2_raised(notice)) { body = notice; break; }
                            json_t *la = json_array();
                            json_append(la, name ? json_copy(name) : json_null());
                            json_append(la, notice ? notice : json_null());
                            json_t *lr = rsu2_log(e, "warning", "Profile %s: %s", la, false);
                            if (rsu2_raised(lr)) { body = lr; break; }
                            json_free(lr);
                        }
                    } else {
                        json_free(rpe);
                    }
                }
                json_free(hits);
                json_free(profiles);
            }
        }
    }
    return rsu2_ls_exit(e, 10, FMT, NULL, true, body);
}

/* ── 16. _start_log_systemd_timing_alignment ──────────────────────────────── */
#define RSU2_STALE_SYSTEMD_FMT \
    "Stale systemd unit detected: %s has TimeoutStopSec=%.0fs but drain_timeout=%.0fs " \
    "cron_drain_timeout=%.0fs (expected >=%.0fs). systemd may SIGKILL the gateway " \
    "mid-drain. Run `hermes gateway install --force` to regenerate the unit, or shorten " \
    "agent.restart_drain_timeout / agent.cron_drain_timeout."
/* PoP: rsu2_start_log_systemd_timing_alignment @ gateway/run_startup.py:_start_log_systemd_timing_alignment */
static json_t *rsu2_start_log_systemd_timing_alignment(rsu2_env *e) {
    static const char *FMT = "check_systemd_timing_alignment failed: %s";
    json_t *ent = rsu2_ls_enter(e, 10, FMT, NULL, false);
    if (rsu2_raised(ent)) return ent;
    json_free(ent);
    json_t *body = NULL;
    json_t *err = NULL;
    const json_t *rdt = rsu2_attr(e->state, "_restart_drain_timeout", &err);
    if (err) {
        body = err;
    } else {
        const json_t *cdt = rsu2_getattr(e->state, "_cron_drain_timeout", NULL);
        json_t *a = rsu2_args();
        rsu2_j(a, "drain_timeout", rdt);
        if (cdt) rsu2_j(a, "cron_drain_timeout", cdt);
        else rsu2_n(a, "cron_drain_timeout", RSU2_DEFAULT_CRON_DRAIN_TIMEOUT);
        json_t *align = rsu2_call(e, "shutdown_forensics.check_systemd_timing_alignment", a);
        if (rsu2_raised(align)) {
            body = align;
        } else if (rsu2_nonnull(align)) {
            if (!rsu2_is_obj(align)) {
                char buf[192];
                snprintf(buf, sizeof(buf), "'%s' object has no attribute 'get'",
                         rsu2_typename(align));
                json_free(align);
                body = rsu2_raise("AttributeError", buf);
            } else {
                const json_t *mismatch = json_obj_get(align, "mismatch");
                if (rsu2_truth(mismatch)) {
                    /* logger.warning args evaluated left-to-right */
                    const json_t *unit = json_obj_get(align, "unit");
                    const json_t *tss = json_obj_get(align, "timeout_stop_sec");
                    const json_t *dt = json_obj_get(align, "drain_timeout");
                    const json_t *cdt2 = json_obj_get(align, "cron_drain_timeout");
                    const json_t *emin = json_obj_get(align, "expected_min");
                    if (!tss) {
                        body = rsu2_raise("KeyError", "'timeout_stop_sec'");
                    } else if (!dt) {
                        body = rsu2_raise("KeyError", "'drain_timeout'");
                    } else if (!emin) {
                        body = rsu2_raise("KeyError", "'expected_min'");
                    } else {
                        json_t *la = json_array();
                        json_append(la, unit ? json_copy(unit) : json_string("(unknown)"));
                        json_append(la, json_copy(tss));
                        json_append(la, json_copy(dt));
                        if (cdt2) json_append(la, json_copy(cdt2));
                        else json_append(la, json_number(RSU2_DEFAULT_CRON_DRAIN_TIMEOUT));
                        json_append(la, json_copy(emin));
                        json_t *lr = rsu2_log(e, "warning", RSU2_STALE_SYSTEMD_FMT, la, false);
                        if (rsu2_raised(lr)) body = lr;
                        else json_free(lr);
                    }
                }
                json_free(align);
            }
        } else {
            json_free(align);
        }
    }
    return rsu2_ls_exit(e, 10, FMT, NULL, false, body);
}

/* ── 17. _open_faulthandler_log ───────────────────────────────────────────── */
/* PoP: rsu2_open_faulthandler_log @ gateway/run_startup.py:_open_faulthandler_log */
static json_t *rsu2_open_faulthandler_log(rsu2_env *e) {
    json_t *err = NULL;
    const json_t *config = rsu2_attr(e->state, "config", &err);
    if (err) return err;
    const json_t *ld = rsu2_getattr(config, "log_dir", NULL);
    char *log_dir = NULL;
    if (rsu2_truth(ld)) {
        log_dir = strdup(rsu2_sval(ld));
    } else {
        json_t *hh = rsu2_call(e, "run.get_hermes_home", rsu2_args());
        if (rsu2_raised(hh)) return hh;
        char *hs = rsu2_py_str(hh);
        json_free(hh);
        log_dir = rsu2_posix_join(hs, "logs");
        free(hs);
    }
    json_t *a1 = rsu2_args();
    rsu2_s(a1, "path", log_dir);
    rsu2_b(a1, "exist_ok", true);
    json_t *mk = rsu2_call(e, "os.makedirs", a1);
    if (rsu2_raised(mk)) { free(log_dir); return mk; }
    json_free(mk);
    char *path = rsu2_posix_join(log_dir, "gateway_faulthandler.log");
    free(log_dir);
    json_t *a2 = rsu2_args();
    rsu2_s(a2, "path", path);
    rsu2_s(a2, "mode", "a");
    rsu2_s(a2, "encoding", "utf-8");
    free(path);
    return rsu2_call(e, "open", a2);
}

/* ── 18. _start_install_faulthandler ──────────────────────────────────────── */
/* PoP: rsu2_start_install_faulthandler @ gateway/run_startup.py:_start_install_faulthandler */
static json_t *rsu2_start_install_faulthandler(rsu2_env *e) {
    json_t *en = rsu2_call(e, "faulthandler.enable", rsu2_args());
    if (rsu2_raised(en)) {
        const char *k = rsu2_kind(en);
        if (rsu2_exc_is(k, "RuntimeError") || rsu2_exc_is(k, "ValueError")
            || rsu2_exc_is(k, "OSError")) {
            json_free(en);
            static const char *FMT = "faulthandler.enable() unavailable";
            json_t *ent = rsu2_ls_enter(e, 10, FMT, NULL, true);
            if (rsu2_raised(ent)) return ent;
            json_free(ent);
            json_t *fh = rsu2_open_faulthandler_log(e);
            json_t *body;
            if (rsu2_raised(fh)) {
                body = fh;
            } else {
                json_t *a = rsu2_args();
                rsu2_j(a, "file", fh);
                json_free(fh);
                rsu2_b(a, "all_threads", true);
                body = rsu2_call(e, "faulthandler.enable", a);
            }
            json_t *ex = rsu2_ls_exit(e, 10, FMT, NULL, true, body);
            if (ex) return ex;
        } else {
            return en;   /* other exception kinds propagate uncaught */
        }
    } else {
        json_free(en);
    }
    /* _sigusr2 = getattr(signal, "SIGUSR2", None) — attribute-read seam */
    json_t *sig = rsu2_call(e, "signal.SIGUSR2", rsu2_args());
    if (rsu2_raised(sig)) return sig;
    if (rsu2_nonnull(sig) && e->has_register) {
        static const char *FMT = "Could not set up faulthandler file logging";
        json_t *ent = rsu2_ls_enter(e, 10, FMT, NULL, true);
        if (rsu2_raised(ent)) { json_free(sig); return ent; }
        json_free(ent);
        json_t *fh = rsu2_open_faulthandler_log(e);
        json_t *body;
        if (rsu2_raised(fh)) {
            body = fh;
        } else {
            json_t *a = rsu2_args();
            rsu2_j(a, "sigusr2", sig);
            rsu2_j(a, "file", fh);
            json_free(fh);
            rsu2_b(a, "all_threads", true);
            rsu2_b(a, "chain", false);
            body = rsu2_call(e, "faulthandler.register", a);
        }
        json_free(sig);
        json_t *ex = rsu2_ls_exit(e, 10, FMT, NULL, true, body);
        if (ex) return ex;
    } else {
        json_free(sig);
    }
    return NULL;
}

/* ── 19. _start_flush_runtime_status (staticmethod async) ─────────────────── */
/* PoP: rsu2_start_flush_runtime_status @ gateway/run_startup.py:_start_flush_runtime_status */
static json_t *rsu2_start_flush_runtime_status(rsu2_env *e) {
    json_t *a = rsu2_args();
    rsu2_n(a, "timeout", 2.0);
    json_t *r = rsu2_call(e, "status.flush_runtime_status_async", a);
    json_t *body = NULL;
    if (rsu2_raised(r)) {
        body = r;
    } else {
        bool ok = rsu2_truth(r);
        json_free(r);
        if (!ok) {
            /* logger.warning is INSIDE the live try region */
            json_t *lr = rsu2_log(e, "warning",
                                  "Timed out flushing final startup runtime status",
                                  NULL, false);
            if (rsu2_raised(lr)) body = lr;
            else json_free(lr);
        }
    }
    if (body) {
        if (rsu2_swallow_exc(body)) return body;   /* BaseException passes */
        json_t *lr = rsu2_log(e, "debug", "Final startup runtime-status flush failed",
                              NULL, true);
        if (rsu2_raised(lr)) return lr;
        json_free(lr);
    }
    return NULL;
}

/* ── 20. _start_check_access_policy ───────────────────────────────────────── */
#define RSU2_NO_ALLOWLIST_FMT \
    "No env user allowlists configured. Messaging platforms default to pairing/allowlist " \
    "policies and will deny unknown senders unless you configure platform allowlists (e.g., " \
    "TELEGRAM_ALLOWED_USERS=your_id) or explicitly opt in with GATEWAY_ALLOW_ALL_USERS=true " \
    "plus dm_policy/group_policy: open on the platform."
#define RSU2_REFUSE_FMT \
    "Refusing to start: %s has dm_policy/group_policy set to 'open' " \
    "but neither GATEWAY_ALLOW_ALL_USERS nor %s is enabled."
/* PoP: rsu2_start_check_access_policy @ gateway/run_startup.py:_start_check_access_policy */
static json_t *rsu2_start_check_access_policy(rsu2_env *e) {
    json_t *allowed = json_array();
    json_t *allow_all = json_array();
    json_t *fail = NULL;
    bool result = false;
    size_t np = sizeof(RSU2_ALLOWLIST_ENV_PLATFORMS)/sizeof(char*);
    for (size_t i = 0; i < np; i++) {
        char buf[96];
        snprintf(buf, sizeof(buf), "%s_ALLOWED_USERS", RSU2_ALLOWLIST_ENV_PLATFORMS[i]);
        json_append(allowed, json_string(buf));
    }
    json_append(allowed, json_string("SIGNAL_GROUP_ALLOWED_USERS"));
    json_append(allowed, json_string("TELEGRAM_GROUP_ALLOWED_USERS"));
    json_append(allowed, json_string("TELEGRAM_GROUP_ALLOWED_CHATS"));
    json_append(allowed, json_string("GATEWAY_ALLOWED_USERS"));
    json_append(allow_all, json_string("GATEWAY_ALLOW_ALL_USERS"));
    for (size_t i = 0; i < np; i++) {
        char buf[96];
        snprintf(buf, sizeof(buf), "%s_ALLOW_ALL_USERS", RSU2_ALLOWLIST_ENV_PLATFORMS[i]);
        json_append(allow_all, json_string(buf));
    }
    /* with suppress(Exception): plugin platform entries */
    {
        json_t *supp = NULL;
        json_t *entries = rsu2_call(e, "platform_registry.loaded_entries", rsu2_args());
        if (rsu2_raised(entries)) {
            supp = entries;
        } else if (!rsu2_is_arr(entries)) {
            char buf[192];
            snprintf(buf, sizeof(buf), "'%s' object is not iterable", rsu2_typename(entries));
            json_free(entries);
            supp = rsu2_raise("TypeError", buf);
        } else {
            json_t *err = NULL;
            const json_t *config = rsu2_attr(e->state, "config", &err);
            const json_t *platforms = NULL;
            if (!err) platforms = rsu2_attr(config, "platforms", &err);
            if (!err && !rsu2_is_obj(platforms)) {
                char buf[192];
                snprintf(buf, sizeof(buf), "'%s' object has no attribute 'items'",
                         rsu2_typename(platforms));
                err = rsu2_raise("AttributeError", buf);
            }
            if (err) {
                supp = err;
            } else {
                for (size_t i = 0; i < platforms->c.count && !supp; i++) {
                    const char *key = platforms->c.keys[i];
                    const json_t *cfg = platforms->c.items[i];
                    json_t *e2 = NULL;
                    const json_t *enabled = rsu2_attr(cfg, "enabled", &e2);
                    if (e2) { supp = e2; break; }
                    if (!rsu2_truth(enabled)) continue;
                    json_t *a = rsu2_args();
                    rsu2_s(a, "value", key);
                    json_t *ent = rsu2_call(e, "platform_registry.get", a);
                    if (rsu2_raised(ent)) { supp = ent; break; }
                    if (rsu2_truth(ent)) json_append(entries, ent);  /* filter(None, ...) */
                    else json_free(ent);
                }
            }
            if (!supp) {
                /* [e for e in entries if e.source == "plugin"] */
                json_t *plugins = json_array();
                for (size_t i = 0; i < json_len(entries) && !supp; i++) {
                    const json_t *ent = json_get(entries, i);
                    json_t *e2 = NULL;
                    const json_t *src = rsu2_attr(ent, "source", &e2);
                    if (e2) { supp = e2; break; }
                    if (rsu2_is_str(src) && strcmp(rsu2_sval(src), "plugin") == 0)
                        json_append(plugins, json_copy((json_t *)ent));
                }
                for (size_t i = 0; i < json_len(plugins) && !supp; i++) {
                    const json_t *ent = json_get(plugins, i);
                    json_t *e2 = NULL;
                    const json_t *aue = rsu2_attr(ent, "allowed_users_env", &e2);
                    if (e2) { supp = e2; break; }
                    if (rsu2_truth(aue)) json_append(allowed, json_copy((json_t *)aue));
                }
                for (size_t i = 0; i < json_len(plugins) && !supp; i++) {
                    const json_t *ent = json_get(plugins, i);
                    json_t *e2 = NULL;
                    const json_t *aae = rsu2_attr(ent, "allow_all_env", &e2);
                    if (e2) { supp = e2; break; }
                    if (rsu2_truth(aae)) json_append(allow_all, json_copy((json_t *)aae));
                }
                json_free(plugins);
            }
            json_free(entries);
        }
        if (supp && rsu2_swallow_exc(supp)) { fail = supp; goto out; }
    }
    /* has_messaging = any(cfg.enabled and platform not in non_messaging ...) */
    {
        json_t *err = NULL;
        const json_t *config = rsu2_attr(e->state, "config", &err);
        const json_t *platforms = NULL;
        if (!err) platforms = rsu2_attr(config, "platforms", &err);
        if (!err && !rsu2_is_obj(platforms)) {
            char buf[192];
            snprintf(buf, sizeof(buf), "'%s' object has no attribute 'items'",
                     rsu2_typename(platforms));
            err = rsu2_raise("AttributeError", buf);
        }
        if (err) { fail = err; goto out; }
        bool has_messaging = false;
        for (size_t i = 0; i < platforms->c.count && !has_messaging; i++) {
            const char *key = platforms->c.keys[i];
            const json_t *cfg = platforms->c.items[i];
            json_t *e2 = NULL;
            const json_t *enabled = rsu2_attr(cfg, "enabled", &e2);
            if (e2) { fail = e2; goto out; }
            if (rsu2_truth(enabled)
                && strcmp(key, "local") != 0 && strcmp(key, "api_server") != 0
                && strcmp(key, "webhook") != 0)
                has_messaging = true;
        }
        if (has_messaging) {
            bool any_allowed = false;
            for (size_t i = 0; i < json_len(allowed) && !any_allowed; i++) {
                json_t *a = rsu2_args();
                rsu2_s(a, "name", rsu2_sval(json_get(allowed, i)));
                json_t *r = rsu2_call(e, "os.getenv", a);
                if (rsu2_raised(r)) { fail = r; goto out; }
                any_allowed = rsu2_truth(r);
                json_free(r);
            }
            if (!any_allowed) {
                bool any_all = false;
                for (size_t i = 0; i < json_len(allow_all) && !any_all; i++) {
                    json_t *a = rsu2_args();
                    rsu2_s(a, "name", rsu2_sval(json_get(allow_all, i)));
                    rsu2_s(a, "default", "");
                    json_t *r = rsu2_call(e, "os.getenv", a);
                    if (rsu2_raised(r)) { fail = r; goto out; }
                    if (!rsu2_is_str(r)) {
                        char buf[192];
                        snprintf(buf, sizeof(buf), "'%s' object has no attribute 'lower'",
                                 rsu2_typename(r));
                        json_free(r);
                        fail = rsu2_raise("AttributeError", buf);
                        goto out;
                    }
                    char *low = rsu2_lower(rsu2_sval(r));
                    json_free(r);
                    any_all = strcmp(low, "true") == 0 || strcmp(low, "1") == 0
                              || strcmp(low, "yes") == 0;
                    free(low);
                }
                if (!any_all) {
                    json_t *lr = rsu2_log(e, "warning", RSU2_NO_ALLOWLIST_FMT, NULL, false);
                    if (rsu2_raised(lr)) { fail = lr; goto out; }
                    json_free(lr);
                }
            }
        }
    }
    /* reason = _own_policy_open_startup_violation(self.config) */
    {
        json_t *err = NULL;
        const json_t *config = rsu2_attr(e->state, "config", &err);
        if (err) { fail = err; goto out; }
        json_t *a = rsu2_args();
        rsu2_j(a, "config", config);
        json_t *reason = rsu2_call(e, "run._own_policy_open_startup_violation", a);
        if (rsu2_raised(reason)) { fail = reason; goto out; }
        if (rsu2_truth(reason)) {
            if (!rsu2_is_str(reason)) {
                char buf[192];
                snprintf(buf, sizeof(buf), "'%s' object has no attribute 'split'",
                         rsu2_typename(reason));
                json_free(reason);
                fail = rsu2_raise("AttributeError", buf);
                goto out;
            }
            const char *rs = rsu2_sval(reason);
            const char *colon = strchr(rs, ':');
            size_t pnlen = colon ? (size_t)(colon - rs) : strlen(rs);
            char *pvalue = malloc(pnlen + 1);
            memcpy(pvalue, rs, pnlen); pvalue[pnlen] = '\0';
            json_t *pv_node = json_string(pvalue);
            const char *validated = NULL;
            json_t *ctor_err = rsu2_platform_ctor(pv_node, &validated);
            if (ctor_err) {
                json_free(pv_node); free(pvalue); json_free(reason);
                fail = ctor_err; goto out;
            }
            json_t *envs = rsu2_own_policy_envs(e, pv_node);
            json_free(pv_node);
            if (rsu2_raised(envs)) {
                free(pvalue); json_free(reason);
                fail = envs; goto out;
            }
            const json_t *allow_all_env = NULL;
            if (rsu2_truth(envs)) {
                if (!rsu2_is_arr(envs)) {
                    char buf[192];
                    snprintf(buf, sizeof(buf), "'%s' object is not subscriptable",
                             rsu2_typename(envs));
                    json_free(envs); free(pvalue); json_free(reason);
                    fail = rsu2_raise("TypeError", buf); goto out;
                }
                if (json_len(envs) < 3) {
                    json_free(envs); free(pvalue); json_free(reason);
                    fail = rsu2_raise("IndexError", "list index out of range"); goto out;
                }
                allow_all_env = json_get(envs, 2);
            }
            json_t *la = json_array();
            json_append(la, json_string(pvalue));
            json_append(la, rsu2_truth(allow_all_env)
                ? json_copy((json_t *)allow_all_env)
                : json_string("a platform allow-all flag"));
            json_t *lr = rsu2_log(e, "error", RSU2_REFUSE_FMT, la, false);
            if (rsu2_raised(lr)) {
                json_free(envs); free(pvalue); json_free(reason);
                fail = lr; goto out;
            }
            json_free(lr);
            json_t *a1 = rsu2_args();
            rsu2_s(a1, "gateway_state", "startup_failed");
            rsu2_j(a1, "exit_reason", reason);
            json_t *w = rsu2_call(e, "run._write_runtime_status_quiet", a1);
            if (rsu2_raised(w)) {
                json_free(envs); free(pvalue); json_free(reason);
                fail = w; goto out;
            }
            json_free(w);
            json_t *a2 = rsu2_args();
            rsu2_j(a2, "reason", reason);
            json_t *c = rsu2_call(e, "self._request_clean_exit", a2);
            if (rsu2_raised(c)) {
                json_free(envs); free(pvalue); json_free(reason);
                fail = c; goto out;
            }
            json_free(c);
            json_free(envs); free(pvalue); json_free(reason);
            result = true;
        } else {
            json_free(reason);
        }
    }
out:
    json_free(allowed);
    json_free(allow_all);
    if (fail) return fail;
    return json_bool(result);
}

/* ── 21. _handoff_resolve_scope ───────────────────────────────────────────── */
/* PoP: rsu2_handoff_resolve_scope @ gateway/run_startup.py:_handoff_resolve_scope */
static json_t *rsu2_handoff_resolve_scope(rsu2_env *e, const json_t *profile_name) {
    const char *pn = rsu2_strval(profile_name);
    if (!rsu2_truth(profile_name) || (pn && strcmp(pn, "default") == 0)) {
        json_t *err = NULL;
        const json_t *config = rsu2_attr(e->state, "config", &err);
        if (err) return err;
        const json_t *adapters = rsu2_attr(e->state, "adapters", &err);
        if (err) return err;
        json_t *out = json_array();
        json_append(out, json_copy((json_t *)config));
        json_append(out, json_copy((json_t *)adapters));
        return out;
    }
    json_t *err = NULL;
    const json_t *pa = rsu2_attr(e->state, "_profile_adapters", &err);
    if (err) return err;
    const json_t *secondary = NULL;
    if (rsu2_truth(pa) && rsu2_is_obj(pa))
        secondary = json_obj_get(pa, pn ? pn : "");
    if (!rsu2_truth(secondary)) {
        char *pns = rsu2_py_str(profile_name);
        size_t n = strlen(pns) + 64;
        char *msg = malloc(n);
        snprintf(msg, n, "profile '%s' has no live adapters in this gateway", pns);
        free(pns);
        json_t *r = rsu2_raise("RuntimeError", msg);
        free(msg);
        return r;
    }
    json_t *cfg = rsu2_call(e, "run.load_gateway_config", rsu2_args());
    if (rsu2_raised(cfg)) {
        /* Snapshot str(exc) BEFORE rsu2_swallow_exc frees the spec on the
         * Exception path — the except body formats it into the RuntimeError.
         * Reading cfg->message after the helper freed it is a UAF, and a
         * second json_free(cfg) here is a double-free (0xC0000374). */
        char *em = strdup(rsu2_msg(cfg));
        if (rsu2_swallow_exc(cfg)) { free(em); return cfg; }
        json_t *la = json_array();
        json_append(la, profile_name ? json_copy((json_t *)profile_name) : json_null());
        json_t *lr = rsu2_log(e, "error",
            "Handoff: could not load config for profile %s; failing the handoff instead of "
            "delivering via the primary's config", la, true);
        if (rsu2_raised(lr)) { free(em); return lr; }
        json_free(lr);
        char *pns = rsu2_py_str(profile_name);
        size_t n = strlen(pns) + strlen(em) + 64;
        char *msg = malloc(n);
        snprintf(msg, n, "could not load config for profile '%s': %s", pns, em);
        free(pns);
        free(em);
        json_t *r = rsu2_raise("RuntimeError", msg);
        free(msg);
        return r;
    }
    json_t *out = json_array();
    json_append(out, cfg);
    json_append(out, json_copy((json_t *)secondary));
    return out;
}

/* ── 22. _handoff_home_chat_type (staticmethod async) ─────────────────────── */
/* PoP: rsu2_handoff_home_chat_type @ gateway/run_startup.py:_handoff_home_chat_type */
static json_t *rsu2_handoff_home_chat_type(rsu2_env *e, const json_t *adapter,
                                           const char *home_chat_id, const char *pvalue) {
    (void)adapter;
    json_t *a = rsu2_args();
    rsu2_s(a, "home_chat_id", home_chat_id);
    json_t *info = rsu2_call(e, "adapter.get_chat_info", a);
    if (rsu2_raised(info)) {
        if (rsu2_swallow_exc(info)) return info;
        json_t *la = rsu2_args1s(home_chat_id);
        json_t *lr = rsu2_log(e, "debug", "Handoff: get_chat_info failed for %s", la, true);
        if (rsu2_raised(lr)) return lr;
        json_free(lr);
        info = NULL;
    }
    char *kind = strdup("");
    if (rsu2_is_obj(info)) {
        const json_t *t = json_obj_get(info, "type");
        if (rsu2_truth(t)) {
            char *ts = rsu2_py_str(t);
            char *st = rsu2_strip(ts);
            free(ts);
            free(kind);
            kind = rsu2_lower(st);
            free(st);
        }
    }
    json_free(info);
    const char *ret;
    if (strcmp(pvalue, "matrix") == 0 || strcmp(pvalue, "discord") == 0) {
        ret = strcmp(kind, "dm") == 0 ? "dm" : "group";
    } else {
        ret = "dm";
        for (size_t i = 0; i < sizeof(RSU2_HANDOFF_SHARED_CHAT_TYPES)/sizeof(char*); i++)
            if (strcmp(kind, RSU2_HANDOFF_SHARED_CHAT_TYPES[i]) == 0) { ret = kind; break; }
    }
    json_t *out = json_string(ret);
    free(kind);
    return out;
}

/* ── 23. _start_aggregate_connect_results (async) ─────────────────────────── */
/* PoP: rsu2_start_aggregate_connect_results @ gateway/run_startup.py:_start_aggregate_connect_results */
static json_t *rsu2_start_aggregate_connect_results(rsu2_env *e, const json_t *raw) {
    if (!rsu2_is_arr(raw)) {
        char buf[192];
        snprintf(buf, sizeof(buf), "'%s' object is not iterable", rsu2_typename(raw));
        return rsu2_raise("TypeError", buf);
    }
    double connected = 0;
    for (size_t i = 0; i < json_len(raw); i++) {
        const json_t *item = json_get(raw, i);
        if (rsu2_raised(item)) {
            /* isinstance(_item, Exception) — fixture domain: Exception family */
            json_t *la = json_array();
            json_append(la, json_copy((json_t *)item));
            json_t *lr = rsu2_log(e, "error", "Unexpected startup connect error: %s", la, false);
            if (rsu2_raised(lr)) return lr;
            json_free(lr);
            continue;
        }
        if (!rsu2_is_arr(item)) {
            char buf[192];
            snprintf(buf, sizeof(buf), "cannot unpack non-iterable %s object",
                     rsu2_typename(item));
            return rsu2_raise("TypeError", buf);
        }
        if (json_len(item) != 5) {
            char buf[192];
            if (json_len(item) < 5)
                snprintf(buf, sizeof(buf),
                         "not enough values to unpack (expected 5, got %zu)", json_len(item));
            else
                snprintf(buf, sizeof(buf),
                         "too many values to unpack (expected 5, got %zu)", json_len(item));
            return rsu2_raise("ValueError", buf);
        }
        const char *pvalue = rsu2_sval(json_get(item, 0));
        const json_t *adapter = json_get(item, 1);
        const json_t *pcfg = json_get(item, 2);
        const char *outcome = rsu2_sval(json_get(item, 3));
        const json_t *exc = json_get(item, 4);
        if (strcmp(outcome, "aborted") == 0) continue;
        if (strcmp(outcome, "exception") == 0) {
            json_t *la = json_array();
            json_append(la, json_string(pvalue));
            json_append(la, exc ? json_copy((json_t *)exc) : json_null());
            json_t *lr = rsu2_log(e, "error", "\xe2\x9c\x97 %s error: %s", la, false);
            if (rsu2_raised(lr)) return lr;
            json_free(lr);
            json_t *a = rsu2_args();
            rsu2_j(a, "adapter", adapter);
            rsu2_s(a, "platform", pvalue);
            json_t *r = rsu2_call(e, "self._safe_adapter_disconnect", a);
            if (rsu2_raised(r)) return r;
            json_free(r);
            char *exc_str = rsu2_raised(exc) ? strdup(rsu2_msg(exc)) : rsu2_py_str(exc);
            json_t *q = rsu2_startup_queue_transient_failure(e, pvalue, adapter, pcfg, exc_str);
            free(exc_str);
            if (rsu2_raised(q)) return q;
            json_free(q);
            continue;
        }
        if (strcmp(outcome, "ok") == 0) {
            json_t *a = rsu2_args();
            rsu2_s(a, "platform", pvalue);
            rsu2_j(a, "adapter", adapter);
            json_t *r = rsu2_call(e, "self._publish_primary_adapter", a);
            if (rsu2_raised(r)) return r;
            json_free(r);
            connected += 1;
            json_t *err = NULL;
            const json_t *deg = rsu2_attr(adapter, "send_path_degraded", &err);
            if (err) return err;
            bool degraded = rsu2_truth(deg);
            const json_t *dsm = NULL;
            if (degraded) {
                dsm = rsu2_attr(adapter, "DEGRADED_STATUS_MESSAGE", &err);
                if (err) return err;
            }
            json_t *a2 = rsu2_args();
            rsu2_s(a2, "value", pvalue);
            rsu2_s(a2, "platform_state", degraded ? "retrying" : "connected");
            rsu2_s(a2, "error_code", NULL);
            rsu2_j(a2, "error_message", dsm);
            json_t *r2 = rsu2_call(e, "self._update_platform_runtime_status", a2);
            if (rsu2_raised(r2)) return r2;
            json_free(r2);
            json_t *la = json_array();
            json_append(la, json_string(pvalue));
            json_append(la, json_string(degraded ? " (degraded)" : ""));
            json_t *lr = rsu2_log(e, "info", "\xe2\x9c\x93 %s connected%s", la, false);
            if (rsu2_raised(lr)) return lr;
            json_free(lr);
            continue;
        }
        /* outcome == "failed" (any other value falls through here, as live) */
        {
            json_t *la = rsu2_args1s(pvalue);
            json_t *lr = rsu2_log(e, "warning", "\xe2\x9c\x97 %s failed to connect", la, false);
            if (rsu2_raised(lr)) return lr;
            json_free(lr);
        }
        {
            json_t *a = rsu2_args();
            rsu2_j(a, "adapter", adapter);
            rsu2_s(a, "platform", pvalue);
            json_t *r = rsu2_call(e, "self._safe_adapter_disconnect", a);
            if (rsu2_raised(r)) return r;
            json_free(r);
        }
        json_t *err = NULL;
        const json_t *hfe = rsu2_attr(adapter, "has_fatal_error", &err);
        if (err) return err;
        if (!rsu2_truth(hfe)) {
            json_t *q = rsu2_startup_queue_transient_failure(e, pvalue, adapter, pcfg,
                                                             "failed to connect");
            if (rsu2_raised(q)) return q;
            json_free(q);
            continue;
        }
        const json_t *fec = rsu2_attr(adapter, "fatal_error_code", &err);
        if (err) return err;
        json_t *a3 = rsu2_args();
        rsu2_j(a3, "code", fec);
        json_t *cr = rsu2_call(e, "restart.is_global_startup_conflict", a3);
        if (rsu2_raised(cr)) return cr;
        bool conflict = rsu2_truth(cr);
        json_free(cr);
        if (conflict)
            json_set(e->state, "_startup_ownership_conflict", json_bool(true));
        const json_t *fer = rsu2_attr(adapter, "fatal_error_retryable", &err);
        if (err) return err;
        bool retryable = rsu2_truth(fer) && !conflict;
        const json_t *fem = rsu2_attr(adapter, "fatal_error_message", &err);
        if (err) return err;
        json_t *a4 = rsu2_args();
        rsu2_s(a4, "value", pvalue);
        rsu2_s(a4, "platform_state", retryable ? "retrying" : "fatal");
        rsu2_j(a4, "error_code", fec);
        rsu2_j(a4, "error_message", fem);
        json_t *r4 = rsu2_call(e, "self._update_platform_runtime_status", a4);
        if (rsu2_raised(r4)) return r4;
        json_free(r4);
        {
            char *fems = rsu2_py_str(fem);
            size_t n = strlen(pvalue) + strlen(fems) + 3;
            char *line = malloc(n);
            snprintf(line, n, "%s: %s", pvalue, fems);
            free(fems);
            json_t *errs = rsu2_st_arr(e, retryable ? "startup_retryable_errors"
                                                    : "startup_nonretryable_errors");
            if (!errs) {
                errs = json_array();
                json_set(e->state, retryable ? "startup_retryable_errors"
                                             : "startup_nonretryable_errors", errs);
            }
            json_append(errs, json_string(line));
            free(line);
        }
        if (retryable) {
            json_t *entry = rsu2_startup_retry_entry(e, pvalue, adapter, pcfg, false);
            if (rsu2_raised(entry)) return entry;
            json_t *fp = rsu2_st_obj(e, "_failed_platforms");
            if (!fp) { fp = json_object(); json_set(e->state, "_failed_platforms", fp); }
            json_set(fp, pvalue, entry ? entry : json_null());
        }
    }
    return json_number(connected);
}

/* ── 24. _start_secondary_profiles (async) ────────────────────────────────── */
#define RSU2_SKIPPED_FMT \
    "%s is enabled but no profile (default or secondary) provided a bot credential for " \
    "it \xe2\x80\x94 the platform is not being served. Add its token to the profile that " \
    "should own it, or disable the platform."
/* PoP: rsu2_start_secondary_profiles @ gateway/run_startup.py:_start_secondary_profiles */
static json_t *rsu2_start_secondary_profiles(rsu2_env *e, double connected_count) {
    json_t *rc = rsu2_call(e, "process_registry.restore_completions", rsu2_args());
    if (rsu2_raised(rc)) return rc;
    json_free(rc);
    json_t *pending = NULL;
    json_t *early_ret = NULL;
    json_t *r = rsu2_call(e, "self._start_secondary_profile_adapters", rsu2_args());
    if (rsu2_raised(r)) {
        const char *k = rsu2_kind(r);
        if (rsu2_exc_is(k, "MultiplexConfigError")) {
            const char *msg = rsu2_msg(r);
            json_t *la = rsu2_args1s(msg);
            json_t *lr = rsu2_log(e, "error", "Gateway multiplexer config error: %s", la, false);
            if (rsu2_raised(lr)) {
                pending = lr;
            } else {
                json_free(lr);
                json_t *a = rsu2_args();
                rsu2_s(a, "reason", msg);
                json_t *ff = rsu2_call(e, "self._startup_fail_fatal_config", a);
                if (rsu2_raised(ff)) pending = ff;
                else {
                    json_free(ff);
                    early_ret = json_array();
                    json_append(early_ret, json_bool(true));
                    json_append(early_ret, json_number(connected_count));
                }
            }
            json_free(r);
        } else if (rsu2_is_exception(k)) {
            json_t *la = json_array();
            json_append(la, json_copy(r));
            json_t *lr = rsu2_log(e, "error",
                "Secondary-profile adapter startup failed: %s", la, true);
            if (rsu2_raised(lr)) pending = lr;
            else json_free(lr);
            json_free(r);
            /* falls through to the skipped/warnings loops */
        } else {
            pending = r;   /* BaseException passes (finally still runs) */
        }
    } else {
        double sum = 0;
        json_t *ae = rsu2_iadd(connected_count, r, &sum);
        if (ae) {
            json_t *la = json_array();
            json_append(la, ae);
            json_t *lr = rsu2_log(e, "error",
                "Secondary-profile adapter startup failed: %s", la, true);
            if (rsu2_raised(lr)) pending = lr;
            else json_free(lr);
        } else {
            connected_count = sum;
        }
        json_free(r);
    }
    /* finally: self._platform_lock_takeover_on_start = False */
    json_set(e->state, "_platform_lock_takeover_on_start", json_bool(false));
    if (pending) return pending;
    if (early_ret) return early_ret;
    /* skipped-platform warnings */
    {
        json_t *err = NULL;
        const json_t *pa = rsu2_attr(e->state, "_profile_adapters", &err);
        if (err) return err;
        if (!rsu2_is_obj(pa)) {
            char buf[192];
            snprintf(buf, sizeof(buf), "'%s' object has no attribute 'values'",
                     rsu2_typename(pa));
            return rsu2_raise("AttributeError", buf);
        }
        const json_t *skipped = rsu2_st_arr(e, "multiplex_skipped_platforms");
        for (size_t i = 0; skipped && i < json_len(skipped); i++) {
            const char *sk = rsu2_sval(json_get(skipped, i));
            bool found = false;
            for (size_t j = 0; j < pa->c.count && !found; j++) {
                const json_t *pmap = pa->c.items[j];
                if (!rsu2_is_obj(pmap)) {
                    char buf[192];
                    snprintf(buf, sizeof(buf),
                             "argument of type '%s' is not a container or iterable",
                             rsu2_typename(pmap));
                    return rsu2_raise("TypeError", buf);
                }
                if (json_has(pmap, sk)) found = true;
            }
            if (!found) {
                json_t *la = rsu2_args1s(sk);
                json_t *lr = rsu2_log(e, "warning", RSU2_SKIPPED_FMT, la, false);
                if (rsu2_raised(lr)) return lr;
                json_free(lr);
            }
        }
    }
    /* unserved shared-ingress warnings */
    json_t *w = rsu2_call(e, "self._unserved_shared_ingress_warnings", rsu2_args());
    if (rsu2_raised(w)) return w;
    if (!rsu2_is_arr(w)) {
        char buf[192];
        snprintf(buf, sizeof(buf), "'%s' object is not iterable", rsu2_typename(w));
        json_free(w);
        return rsu2_raise("TypeError", buf);
    }
    for (size_t i = 0; i < json_len(w); i++) {
        const char *line = rsu2_sval(json_get(w, i));
        json_t *lr = rsu2_log(e, "warning", line, NULL, false);
        if (rsu2_raised(lr)) { json_free(w); return lr; }
        json_free(lr);
    }
    json_free(w);
    json_t *out = json_array();
    json_append(out, json_bool(false));
    json_append(out, json_number(connected_count));
    return out;
}

/* ── dispatch wrappers (extract call args from in; validate Platform where
 *    the oracle pre-materializes it BEFORE the live call) ────────────────── */
static json_t *rsu2_drv_own_policy_envs(rsu2_env *e, const json_t *in) {
    const json_t *p = json_obj_get(in, "platform");
    const char *pv = NULL;
    json_t *err = rsu2_platform_ctor(p, &pv);
    if (err) return err;
    return rsu2_own_policy_envs(e, p);
}
static json_t *rsu2_drv_log_agent_budget(rsu2_env *e, const json_t *in) {
    (void)in; return rsu2_log_agent_budget(e);
}
static json_t *rsu2_drv_startup_retry_entry(rsu2_env *e, const json_t *in) {
    const json_t *p = json_obj_get(in, "platform");
    const char *pv = NULL;
    json_t *err = rsu2_platform_ctor(p, &pv);
    if (err) return err;
    return rsu2_startup_retry_entry(e, pv, json_obj_get(in, "adapter"),
                                    json_obj_get(in, "platform_config"),
                                    json_get_bool(in, "queued", true));
}
static json_t *rsu2_drv_startup_queue_transient_failure(rsu2_env *e, const json_t *in) {
    const json_t *p = json_obj_get(in, "platform");
    const char *pv = NULL;
    json_t *err = rsu2_platform_ctor(p, &pv);
    if (err) return err;
    return rsu2_startup_queue_transient_failure(e, pv, json_obj_get(in, "adapter"),
                                                json_obj_get(in, "platform_config"),
                                                json_get_str(in, "message", ""));
}
static json_t *rsu2_drv_startup_teardown_adapter(rsu2_env *e, const json_t *in) {
    const json_t *p = json_obj_get(in, "platform");
    const char *pv = NULL;
    json_t *err = rsu2_platform_ctor(p, &pv);
    if (err) return err;
    return rsu2_startup_teardown_adapter(e, json_obj_get(in, "adapter"), pv);
}
static json_t *rsu2_drv_resume_pending_candidates(rsu2_env *e, const json_t *in) {
    const json_t *p = json_obj_get(in, "platform");
    if (rsu2_nonnull(p)) {
        const char *pv = NULL;
        json_t *err = rsu2_platform_ctor(p, &pv);
        if (err) return err;
    }
    return rsu2_resume_pending_candidates(e, p);
}
static json_t *rsu2_drv_release_runtime_claim_quiet(rsu2_env *e, const json_t *in) {
    const json_t *err_node = json_obj_get(in, "error");
    json_t *def = NULL;
    if (!err_node) { def = json_string("send_path_degraded"); err_node = def; }
    json_t *r = rsu2_release_runtime_claim_quiet(e, json_obj_get(in, "obligation_id"),
                                                 json_get_str(in, "log_fmt", ""), err_node);
    json_free(def);
    return r;
}
static json_t *rsu2_drv_obligation_adapter(rsu2_env *e, const json_t *in) {
    return rsu2_obligation_adapter(e, json_obj_get(in, "row"));
}
static json_t *rsu2_drv_on_relay_provisioned(rsu2_env *e, const json_t *in) {
    (void)in; return rsu2_on_relay_provisioned(e);
}
static json_t *rsu2_drv_start_relay_provision_retry(rsu2_env *e, const json_t *in) {
    (void)in; return rsu2_start_relay_provision_retry(e);
}
static json_t *rsu2_drv_register_config_hooks(rsu2_env *e, const json_t *in) {
    return rsu2_register_config_hooks(e, json_get_str(in, "fail_fmt", ""),
                                      json_obj_get(in, "fail_args"),
                                      (int)json_get_num(in, "level", 10));
}
static json_t *rsu2_drv_register_launch_profile_config_hooks(rsu2_env *e, const json_t *in) {
    (void)in; return rsu2_register_launch_profile_config_hooks(e);
}
static json_t *rsu2_drv_start_register_plugins_relay_hooks(rsu2_env *e, const json_t *in) {
    (void)in; return rsu2_start_register_plugins_relay_hooks(e);
}
static json_t *rsu2_drv_recover_secondary_process_checkpoints(rsu2_env *e, const json_t *in) {
    (void)in; return rsu2_recover_secondary_process_checkpoints(e);
}
static json_t *rsu2_drv_start_log_retired_session_reset(rsu2_env *e, const json_t *in) {
    (void)in; return rsu2_start_log_retired_session_reset(e);
}
static json_t *rsu2_drv_start_log_systemd_timing_alignment(rsu2_env *e, const json_t *in) {
    (void)in; return rsu2_start_log_systemd_timing_alignment(e);
}
static json_t *rsu2_drv_open_faulthandler_log(rsu2_env *e, const json_t *in) {
    (void)in; return rsu2_open_faulthandler_log(e);
}
static json_t *rsu2_drv_start_install_faulthandler(rsu2_env *e, const json_t *in) {
    (void)in; return rsu2_start_install_faulthandler(e);
}
static json_t *rsu2_drv_start_flush_runtime_status(rsu2_env *e, const json_t *in) {
    (void)in; return rsu2_start_flush_runtime_status(e);
}
static json_t *rsu2_drv_start_check_access_policy(rsu2_env *e, const json_t *in) {
    (void)in; return rsu2_start_check_access_policy(e);
}
static json_t *rsu2_drv_handoff_resolve_scope(rsu2_env *e, const json_t *in) {
    return rsu2_handoff_resolve_scope(e, json_obj_get(in, "profile_name"));
}
static json_t *rsu2_drv_handoff_home_chat_type(rsu2_env *e, const json_t *in) {
    const json_t *p = json_obj_get(in, "platform");
    const char *pv = NULL;
    json_t *err = rsu2_platform_ctor(p, &pv);
    if (err) return err;
    return rsu2_handoff_home_chat_type(e, json_obj_get(in, "adapter"),
                                       json_get_str(in, "home_chat_id", ""), pv);
}
static json_t *rsu2_drv_start_aggregate_connect_results(rsu2_env *e, const json_t *in) {
    return rsu2_start_aggregate_connect_results(e, json_obj_get(in, "raw"));
}
static json_t *rsu2_drv_start_secondary_profiles(rsu2_env *e, const json_t *in) {
    return rsu2_start_secondary_profiles(e, json_get_num(in, "connected_count", 0));
}

/* ── dispatch table ───────────────────────────────────────────────────────── */
typedef json_t *(*rsu2_fn)(rsu2_env *, const json_t *);
static const struct { const char *name; rsu2_fn fn; } rsu2_table[] = {
    {"_own_policy_envs", rsu2_drv_own_policy_envs},
    {"_log_agent_budget", rsu2_drv_log_agent_budget},
    {"_startup_retry_entry", rsu2_drv_startup_retry_entry},
    {"_startup_queue_transient_failure", rsu2_drv_startup_queue_transient_failure},
    {"_startup_teardown_adapter", rsu2_drv_startup_teardown_adapter},
    {"_resume_pending_candidates", rsu2_drv_resume_pending_candidates},
    {"_release_runtime_claim_quiet", rsu2_drv_release_runtime_claim_quiet},
    {"_obligation_adapter", rsu2_drv_obligation_adapter},
    {"_on_relay_provisioned", rsu2_drv_on_relay_provisioned},
    {"_start_relay_provision_retry", rsu2_drv_start_relay_provision_retry},
    {"_register_config_hooks", rsu2_drv_register_config_hooks},
    {"_register_launch_profile_config_hooks", rsu2_drv_register_launch_profile_config_hooks},
    {"_start_register_plugins_relay_hooks", rsu2_drv_start_register_plugins_relay_hooks},
    {"_recover_secondary_process_checkpoints", rsu2_drv_recover_secondary_process_checkpoints},
    {"_start_log_retired_session_reset", rsu2_drv_start_log_retired_session_reset},
    {"_start_log_systemd_timing_alignment", rsu2_drv_start_log_systemd_timing_alignment},
    {"_open_faulthandler_log", rsu2_drv_open_faulthandler_log},
    {"_start_install_faulthandler", rsu2_drv_start_install_faulthandler},
    {"_start_flush_runtime_status", rsu2_drv_start_flush_runtime_status},
    {"_start_check_access_policy", rsu2_drv_start_check_access_policy},
    {"_handoff_resolve_scope", rsu2_drv_handoff_resolve_scope},
    {"_handoff_home_chat_type", rsu2_drv_handoff_home_chat_type},
    {"_start_aggregate_connect_results", rsu2_drv_start_aggregate_connect_results},
    {"_start_secondary_profiles", rsu2_drv_start_secondary_profiles},
};
