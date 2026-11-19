/*
 * api_server_flow2.c — FLOW port of the UNCLAIMED remainder of
 * gateway/platforms/api_server.py (the aiohttp OpenAI-compatible API server
 * adapter). The claimed siblings live in api_server_flow.c / api_server_gaps.c
 * / api_server_ports.c etc.; the EFFECTFUL claimed siblings are SEAMED here
 * (universal-seam calls) and the PURE claimed leaves are transcribed inline as
 * un-annotated helpers (no PoP re-claim).
 *
 * Ports the DECISION SEQUENCES of 24 functions:
 *   metrics/status — mark_shutdown_requested, _active_structured_run_count,
 *     _resolve_api_server_int, _metrics_day_key, _iso_timestamp,
 *     _reset_metrics_if_needed, _total_tokens_from_usage, _latency_p95_ms,
 *     _api_server_status_payload, _publish_runtime_status, _record_api_metrics,
 *     _heartbeat_loop
 *   provider/model — _resolve_provider_runtime, _apply_provider_runtime,
 *     _recover_or_record_model, _select_agent_runtime,
 *     _ProviderAuthResolutionError.is_rate_limited,
 *     _ProviderAuthResolutionError.user_text
 *   turn metadata  — _effective_turn_runtime, _turn_runtime_metadata,
 *     _finish_turn_result
 *   HTTP handlers  — _handle_health, _handle_models, _handle_capabilities
 *
 * Ported functions call each other REAL (internal composition). Every effectful
 * collaborator (config reads, provider resolution, profile/model lookups, the
 * clock, asyncio.sleep, web.json_response, the runtime-status writer, the runs
 * module delegates, i18n t(), the auth rate-limit probe and the request-profile
 * ContextVar) rides ONE universal seam.
 *
 * Modeling contract (shared with tests/oracle_api_server_flow2.py):
 *  - Universal seam: json_t *(*call)(void *ud, const char *name, args). name
 *    identifies the live call; args is a BORROWED JSON object the stub copies
 *    into the trace; the result is a NEW json_t (NULL == None;
 *    {"__raise__":kind,"message":m} == a raised exception). The port owns args
 *    (as2_call frees them) and the result.
 *  - Seam names -> live calls:
 *      is_connected                 -> self.is_connected property read
 *      time.time                    -> time.time()
 *      asyncio.sleep                -> await asyncio.sleep(seconds)
 *      self._write_runtime_status_safe -> BasePlatformAdapter writer (kwargs)
 *      runs._mark_shutdown_requested -> _api_runs._mark_shutdown_requested(self)
 *      runs._idempotency_capabilities -> _api_runs._idempotency_capabilities(self, store_type=...)
 *      config.load_config / config.cfg_get -> hermes_cli.config in-function import
 *      resolve_request_runtime_agent_kwargs -> claimed module sibling
 *      run._resolve_runtime_agent_kwargs_for_provider -> gateway.run fallback
 *      models.get_default_model_for_provider -> hermes_cli.models
 *      model_switch.resolve_effective_model -> hermes_cli.model_switch
 *      session_model_override_for   -> claimed adapter sibling
 *      resolve_model_name           -> claimed adapter sibling
 *      hermes_version               -> claimed module sibling _hermes_version
 *      auth.is_rate_limited_auth_error -> hermes_cli.auth
 *      t                            -> agent.i18n.t(key, **kwargs)
 *      request_profile              -> _api_request_profile.get()
 *      web.json_response            -> aiohttp web.json_response(data, status=, headers=)
 *  - PURE claimed leaves transcribed inline (run REAL in the oracle, no seam,
 *    no PoP): _clean_request_string, _clean_runtime_id, _requested_ids,
 *    _result_runtime, _model_lock_state, _sanitize_runtime_metadata,
 *    _apply_runtime_agent_overrides.
 *  - State mirror (in["state"], copied by the harness before mutation): host /
 *    port / inflight_agent_runs / run_statuses / metrics_day /
 *    metrics_requests_today / metrics_messages_today / metrics_tokens_today /
 *    metrics_latency_ms / metrics_last_request_at / metrics_last_heartbeat_at /
 *    model_name / last_resolved_model / model_routes / cors_origins /
 *    gateway_runner / runtime_kwargs / result. runtime_kwargs and result are
 *    the CALLER'S mutable arg dicts (provider selection / turn finish write
 *    them in place); they travel in the mirror so out.state captures the exact
 *    mutation history. Seamed siblings do NOT mutate the mirror.
 *  - logger is a SILENT abstracted side effect on both sides (no trace event).
 *  - _ProviderAuthResolutionError is a RuntimeError subclass; its raise spec is
 *    {"__raise__":"_ProviderAuthResolutionError","message":str(cause)}. The
 *    __cause__ chain is carried for the is_rate_limited / user_text drives.
 *  - int()/float()/str() coercions transcribe CPython 3.14 (ASCII digit domain,
 *    integral floats read as ints, finite numbers). round(x,2) uses the
 *    correctly-rounded decimal of the binary double (matches Python round).
 *    _metrics_day_key / _iso_timestamp transcribe time.gmtime+strftime and
 *    datetime.fromtimestamp(ts, UTC).isoformat() (microsecond carry handled).
 *  - FIXTURE DOMAIN (excluded divergences, verified by probes): json_t carries
 *    no int/float identity, so an integral-FLOAT literal (100.0) in a str()
 *    position reads as int ("100") where live str(100.0)=="100.0" — fixtures
 *    keep str()-position numbers integral-int or non-integral. _metrics_day_key
 *    fixtures use non-negative timestamps (Windows time.gmtime rejects negative;
 *    the C civil algorithm is more permissive). _iso_timestamp fixtures stay
 *    within Windows datetime.fromtimestamp's valid range (small negatives OK).
 *    Invalid int() tokens are printable ASCII without quotes/backslashes.
 *  - Exception kinds: `except Exception` / suppress(Exception) swallow the
 *    Exception family and pass the BaseException trio (SystemExit,
 *    KeyboardInterrupt, GeneratorExit) + CancelledError through. The full
 *    builtin hierarchy is tabulated below (as2_exc_parent).
 *  - Scripted seam results: a missing/exhausted script is fail-closed — the
 *    harness dispatcher RETURNS a {"__raise__":"RuntimeError",...} spec and
 *    records it, so the port propagates it like any scripted raise.
 *  - Self-contained: the harness #includes this .c; every symbol is static and
 *    as2_*-prefixed; only libjson is linked.
 *
 * PoP annotations (one per ported function) appear at each definition below.
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
#define AS2_DEFAULT_PORT 8642
#define AS2_LATENCY_SAMPLE_LIMIT 512
static const char AS2_SESSION_SOURCE[] = "api_server";

/* _STATIC_FEATURE_FLAGS (order is part of the JSON shape; canon sorts keys). */
typedef struct { const char *k; int kind; /*0=bool,1=str*/ bool b; const char *s; } as2_flag;
static const as2_flag AS2_STATIC_FEATURE_FLAGS[] = {
    {"run_status",0,true,NULL},{"run_events_sse",0,true,NULL},{"run_stop",0,true,NULL},
    {"run_steer",0,true,NULL},{"run_approval_response",0,true,NULL},
    {"tool_progress_events",0,true,NULL},{"approval_events",0,true,NULL},
    {"session_resources",0,true,NULL},{"model_options",0,true,NULL},{"session_chat",0,true,NULL},
    {"session_chat_streaming",0,true,NULL},{"session_fork",0,true,NULL},{"session_model_lock",0,true,NULL},
    {"reasoning_streaming",0,true,NULL},{"admin_config_rw",0,false,NULL},{"jobs_admin",0,false,NULL},
    {"memory_write_api",0,false,NULL},{"skills_api",0,true,NULL},{"audio_api",0,false,NULL},
    {"realtime_voice",0,false,NULL},
    {"session_continuity_header",1,false,"X-Hermes-Session-Id"},
    {"session_key_header",1,false,"X-Hermes-Session-Key"},
};
/* _CAPABILITY_ENDPOINTS: name -> (method, path[, feature]). */
typedef struct { const char *name, *method, *path, *feature; } as2_endpoint;
static const as2_endpoint AS2_CAPABILITY_ENDPOINTS[] = {
    {"health","GET","/health",NULL},{"health_detailed","GET","/health/detailed",NULL},
    {"models","GET","/v1/models",NULL},{"model_options","GET","/api/model/options",NULL},
    {"chat_completions","POST","/v1/chat/completions",NULL},
    {"responses","POST","/v1/responses",NULL},{"runs","POST","/v1/runs",NULL},
    {"run_status","GET","/v1/runs/{run_id}",NULL},
    {"run_events","GET","/v1/runs/{run_id}/events",NULL},
    {"run_approval","POST","/v1/runs/{run_id}/approval",NULL},
    {"run_steer","POST","/v1/runs/{run_id}/steer",NULL},
    {"run_unknown_resolution","POST","/v1/runs/{run_id}/resolve-unknown","run_unknown_resolution"},
    {"run_stop","POST","/v1/runs/{run_id}/stop",NULL},{"skills","GET","/v1/skills",NULL},
    {"toolsets","GET","/v1/toolsets",NULL},{"sessions","GET","/api/sessions",NULL},
    {"session_create","POST","/api/sessions",NULL},
    {"session","GET","/api/sessions/{session_id}",NULL},
    {"session_update","PATCH","/api/sessions/{session_id}",NULL},
    {"session_delete","DELETE","/api/sessions/{session_id}",NULL},
    {"session_messages","GET","/api/sessions/{session_id}/messages",NULL},
    {"session_fork","POST","/api/sessions/{session_id}/fork",NULL},
    {"session_chat","POST","/api/sessions/{session_id}/chat",NULL},
    {"session_chat_stream","POST","/api/sessions/{session_id}/chat/stream",NULL},
    {"session_model_lock","POST","/api/sessions/{session_id}/model",NULL},
};
/* _RUNTIME_AGENT_OVERRIDE_KEYS */
static const char *AS2_OVERRIDE_KEYS[] = {
    "api_key","base_url","provider","api_mode","command","args","credential_pool"};

/* ── seam environment ─────────────────────────────────────────────────────── */
typedef struct as2_env {
    json_t *state;   /* borrowed mirror (harness-owned copy of in["state"]) */
    json_t *(*call)(void *ud, const char *name, const json_t *args);
    void *ud;
} as2_env;

/* ── JSON type / truthiness helpers ───────────────────────────────────────── */
static bool as2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool as2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool as2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool as2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool as2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *as2_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static const char *as2_sval(const json_t *v) { const char *s = as2_str(v); return s ? s : ""; }
static bool as2_truth(const json_t *v) {
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
/* present and not JSON null (models `x is not None` on a getattr/get result). */
static bool as2_nonnull(const json_t *v) { return v && v->type != JSON_NULL; }
static const char *as2_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == (double)(long long)d && d < 9.2e18 && d > -9.2e18) return "int";
            return "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* ── raise specs + full builtin exception hierarchy ───────────────────────── */
static bool as2_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && as2_is_str(json_obj_get(r, "__raise__"));
}
static const char *as2_kind(const json_t *r) { return json_get_str(r, "__raise__", ""); }
static const char *as2_msg(const json_t *r) { return json_get_str(r, "message", ""); }
static json_t *as2_raise_spec(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "message", json_string(msg ? msg : ""));
    return r;
}
/* FULL builtin hierarchy (child -> parent). Custom _ProviderAuthResolutionError
 * and asyncio.CancelledError included. */
static const struct { const char *c, *p; } AS2_EXC_PARENT[] = {
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
    {"ReferenceError","Exception"},{"RuntimeError","Exception"},
    {"NotImplementedError","RuntimeError"},{"RecursionError","RuntimeError"},
    {"_ProviderAuthResolutionError","RuntimeError"},
    {"SyntaxError","Exception"},{"IndentationError","SyntaxError"},{"TabError","IndentationError"},
    {"SystemError","Exception"},{"TypeError","Exception"},{"ValueError","Exception"},
    {"UnicodeError","ValueError"},{"UnicodeDecodeError","UnicodeError"},
    {"UnicodeEncodeError","UnicodeError"},{"UnicodeTranslateError","UnicodeError"},
    {"Warning","Exception"},{"UserWarning","Warning"},{"DeprecationWarning","Warning"},
    {"PendingDeprecationWarning","Warning"},{"SyntaxWarning","Warning"},
    {"RuntimeWarning","Warning"},{"FutureWarning","Warning"},{"ImportWarning","Warning"},
    {"UnicodeWarning","Warning"},{"BytesWarning","Warning"},{"ResourceWarning","Warning"},
    {"EncodingWarning","Warning"},{"ExceptionGroup","Exception"},
};
static const char *as2_exc_parent(const char *kind) {
    for (size_t i = 0; i < sizeof(AS2_EXC_PARENT)/sizeof(AS2_EXC_PARENT[0]); i++)
        if (strcmp(AS2_EXC_PARENT[i].c, kind) == 0) return AS2_EXC_PARENT[i].p;
    return NULL;
}
/* kind is-a ancestor (walks the parent chain). */
static bool as2_exc_is(const char *kind, const char *ancestor) {
    const char *k = kind;
    while (k) {
        if (strcmp(k, ancestor) == 0) return true;
        k = as2_exc_parent(k);
    }
    return false;
}
static bool as2_is_exception(const char *kind) { return as2_exc_is(kind, "Exception"); }
/* `except Exception`: swallow Exception family (free r), propagate BaseException.
 * Returns true when the caller must propagate (r stays owned). */
static bool as2_swallow_exc(json_t *r) {
    if (!as2_is_exception(as2_kind(r))) return true;
    json_free(r);
    return false;
}

/* ── universal seam call (args owned by port, freed after the call) ───────── */
static json_t *as2_call(as2_env *e, const char *name, json_t *args) {
    json_t *r = e->call(e->ud, name, args);
    json_free(args);
    return r;
}
static json_t *as2_seam_obj(as2_env *e, const char *name) {
    return as2_call(e, name, json_object());
}

/* ── Python str()/repr() coercions (ASCII domain) ─────────────────────────── */
static void as2_float_repr(double v, char *out, size_t cap) {
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
static char *as2_py_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL: return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 9.2e18) {
                snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
                return strdup(buf);
            }
            as2_float_repr(v->num_val, buf, sizeof(buf));
            return strdup(buf);
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        default: return strdup("");
    }
}
static char *as2_py_repr(const char *s) {
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

/* ── Python str.strip() whitespace (ASCII subset; fixtures ASCII) ─────────── */
static bool as2_py_ws(char c) {
    return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f';
}
static char *as2_strip(const char *s) {
    if (!s) return strdup("");
    while (*s && as2_py_ws(*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && as2_py_ws(s[n-1])) n--;
    char *out = malloc(n+1); memcpy(out, s, n); out[n] = '\0';
    return out;
}

/* ── int()/float() with live raise semantics ──────────────────────────────── */
static json_t *as2_py_int(const json_t *v, double *out) {
    if (!v || v->type == JSON_NULL)
        return as2_raise_spec("TypeError",
            "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return NULL; }
    if (v->type == JSON_NUMBER) { *out = trunc(v->num_val); return NULL; }
    if (v->type != JSON_STRING) {
        char buf[192];
        snprintf(buf, sizeof(buf),
            "int() argument must be a string, a bytes-like object or a real number, not '%s'",
            as2_type_name(v));
        return as2_raise_spec("TypeError", buf);
    }
    const char *orig = v->str_val ? v->str_val : "";
    char *clean = as2_strip(orig);
    char *p = clean; bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    char digits[512]; size_t nd = 0;
    bool ok = *p != '\0', prev_digit = false;
    for (; *p; p++) {
        if (*p >= '0' && *p <= '9') { if (nd < sizeof(digits)-1) digits[nd++] = *p; prev_digit = true; }
        else if (*p == '_') {
            if (!prev_digit || !(p[1] >= '0' && p[1] <= '9')) { ok = false; break; }
            prev_digit = false;
        } else { ok = false; break; }
    }
    if (ok && nd > 0) {
        digits[nd] = '\0';
        long long ll = strtoll(digits, NULL, 10);
        *out = neg ? -(double)ll : (double)ll;
        free(clean); return NULL;
    }
    free(clean);
    char *rep = as2_py_repr(orig);
    char buf[1024];
    snprintf(buf, sizeof(buf), "invalid literal for int() with base 10: %s", rep);
    free(rep);
    return as2_raise_spec("ValueError", buf);
}

/* ── round(x, 2): correctly-rounded decimal of the binary double ──────────── */
static double as2_round2(double x) {
    if (!isfinite(x)) return x;
    char buf[64];
    snprintf(buf, sizeof(buf), "%.2f", x);
    return strtod(buf, NULL);
}

/* ── time.gmtime + strftime("%Y-%m-%d") and datetime.isoformat (UTC) ──────── */
/* civil-from-days (Howard Hinnant algorithm), days since 1970-01-01. */
static void as2_civil_from_days(long long z, int *y, unsigned *m, unsigned *d) {
    z += 719468;
    long long era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;
    long long yy = (long long)yoe + era * 400;
    unsigned doy = doe - (365*yoe + yoe/4 - yoe/100);
    unsigned mp = (5*doy + 2)/153;
    unsigned dd = doy - (153*mp + 2)/5 + 1;
    unsigned mm = mp < 10 ? mp + 3 : mp - 9;
    *y = (int)(yy + (mm <= 2)); *m = mm; *d = dd;
}
static void as2_gmtime(double ts, int *y, unsigned *mo, unsigned *d,
                       unsigned *hh, unsigned *mm, unsigned *ss, long *us) {
    double fl = floor(ts);
    double frac = ts - fl;
    long u = (long)floor(frac * 1e6 + 0.5);
    if (u >= 1000000) { u -= 1000000; fl += 1.0; }
    long long t = (long long)fl;
    long long days = t / 86400; long long rem = t % 86400;
    if (rem < 0) { rem += 86400; days -= 1; }
    as2_civil_from_days(days, y, mo, d);
    *hh = (unsigned)(rem / 3600); *mm = (unsigned)((rem % 3600) / 60); *ss = (unsigned)(rem % 60);
    *us = u;
}
static char *as2_day_key(double ts) {
    int y; unsigned mo, d, hh, mm, ss; long us;
    as2_gmtime(ts, &y, &mo, &d, &hh, &mm, &ss, &us);
    char *out = malloc(32);
    snprintf(out, 32, "%04d-%02u-%02u", y, mo, d);
    return out;
}
static char *as2_iso_format(double ts) {
    int y; unsigned mo, d, hh, mm, ss; long us;
    as2_gmtime(ts, &y, &mo, &d, &hh, &mm, &ss, &us);
    char *out = malloc(64);
    if (us)
        snprintf(out, 64, "%04d-%02u-%02uT%02u:%02u:%02u.%06ld+00:00", y, mo, d, hh, mm, ss, us);
    else
        snprintf(out, 64, "%04d-%02u-%02uT%02u:%02u:%02u+00:00", y, mo, d, hh, mm, ss);
    return out;
}

/* ── inline PURE claimed leaves (no PoP re-claim; run REAL in the oracle) ─── */
/* _clean_request_string(value): (value.strip() or None) if str else None. */
static char *as2_clean_request_string(const json_t *value) {
    if (!as2_is_str(value)) return NULL;
    char *s = as2_strip(as2_sval(value));
    if (s[0] == '\0') { free(s); return NULL; }
    return s;   /* malloc'd, caller frees */
}
/* _clean_runtime_id(value, max_len=200): "" if None else str(value).strip();
 * "" if len > max_len or contains \r \n \0. */
static char *as2_clean_runtime_id(const json_t *value, int max_len) {
    char *text;
    if (as2_is_null(value)) text = strdup("");
    else { char *ps = as2_py_str(value); text = as2_strip(ps); free(ps); }
    size_t n = strlen(text);
    bool bad = (long)n > max_len;
    for (size_t i = 0; i < n && !bad; i++)
        if (text[i]=='\r'||text[i]=='\n'||text[i]=='\0') bad = true;
    if (bad) { free(text); return strdup(""); }
    return text;
}
/* _apply_runtime_agent_overrides(runtime_kwargs, overrides): merge override
 * keys in place; args list/tuple -> list. Returns runtime_kwargs (borrowed). */
static void as2_apply_overrides(json_t *runtime_kwargs, const json_t *overrides) {
    if (!as2_is_obj(overrides)) return;
    for (size_t i = 0; i < sizeof(AS2_OVERRIDE_KEYS)/sizeof(char*); i++) {
        const char *key = AS2_OVERRIDE_KEYS[i];
        const json_t *value = json_obj_get(overrides, key);
        if (as2_is_null(value)) continue;
        if (strcmp(key, "args") == 0 && as2_is_arr(value))
            json_set(runtime_kwargs, key, json_copy(value));
        else
            json_set(runtime_kwargs, key, json_copy(value));
    }
}
/* _requested_ids(requested) -> (model, provider) cleaned. */
static void as2_requested_ids(const json_t *requested, char **model, char **provider) {
    const json_t *r = as2_truth(requested) ? requested : NULL;
    const json_t *m = r ? json_obj_get(r, "model") : NULL;
    const json_t *p = r ? json_obj_get(r, "provider") : NULL;
    *model = as2_clean_runtime_id(m, 200);
    *provider = as2_clean_runtime_id(p, 80);
}
/* _result_runtime(result, usage): result.runtime or {} ; fallback usage.runtime. */
static json_t *as2_result_runtime(const json_t *result, const json_t *usage) {
    json_t *runtime = NULL;
    if (as2_is_obj(result)) {
        const json_t *r = json_obj_get(result, "runtime");
        if (as2_truth(r)) runtime = json_copy(r);
    }
    if (!runtime) runtime = json_object();
    if (!as2_truth(runtime) && as2_is_obj(usage)) {
        const json_t *r = json_obj_get(usage, "runtime");
        if (as2_truth(r)) { json_free(runtime); runtime = json_copy(r); }
    }
    return runtime;   /* owned */
}
/* _model_lock_state(runtime_request, runtime): "" / "confirmed" / "accepted". */
static const char *as2_model_lock_state(const json_t *runtime_request, const json_t *runtime) {
    const json_t *req = runtime_request ? json_obj_get(runtime_request, "require_model_lock") : NULL;
    if (!as2_truth(req)) return "";
    return as2_truth(runtime) ? "confirmed" : "accepted";
}
/* _sanitize_runtime_metadata(runtime, requested_runtime, route_source, model_lock). */
static json_t *as2_sanitize_runtime_metadata(const json_t *runtime, const json_t *requested_runtime,
                                             const char *route_source, const char *model_lock) {
    json_t *payload = as2_truth(runtime) ? json_copy(runtime) : json_object();
    const json_t *pv = json_obj_get(payload, "provider");
    if (!as2_truth(pv)) pv = json_obj_get(payload, "provider_id");
    if (!as2_truth(pv)) pv = json_obj_get(payload, "effective_provider");
    char *provider = as2_clean_runtime_id(pv, 80);
    const json_t *mv = json_obj_get(payload, "model");
    if (!as2_truth(mv)) mv = json_obj_get(payload, "model_id");
    if (!as2_truth(mv)) mv = json_obj_get(payload, "effective_model");
    char *model = as2_clean_runtime_id(mv, 200);
    /* source = _clean_runtime_id(payload.route_source or route_source, 64) */
    const json_t *sv = json_obj_get(payload, "route_source");
    char *source;
    if (as2_truth(sv)) source = as2_clean_runtime_id(sv, 64);
    else {
        json_t *rs = json_string(route_source ? route_source : "");
        source = as2_clean_runtime_id(rs, 64); json_free(rs);
    }
    json_t *result = json_object();
    json_set(result, "provider", json_string(provider));
    json_set(result, "model", json_string(model));
    json_set(result, "route_source", json_string(source[0] ? source : "global"));
    const json_t *req_payload = json_obj_get(payload, "requested");
    if (as2_truth(requested_runtime) || as2_truth(req_payload)) {
        const json_t *use = as2_truth(requested_runtime) ? requested_runtime : req_payload;
        char *rm, *rp; as2_requested_ids(use, &rm, &rp);
        json_t *req = json_object();
        json_set(req, "provider", json_string(rp));
        json_set(req, "model", json_string(rm));
        json_set(result, "requested", req);
        free(rm); free(rp);
    }
    const json_t *ml_payload = json_obj_get(payload, "model_lock");
    if ((model_lock && model_lock[0]) || as2_truth(ml_payload)) {
        const json_t *use = (model_lock && model_lock[0]) ? NULL : ml_payload;
        char *ml;
        if (model_lock && model_lock[0]) {
            json_t *s = json_string(model_lock); ml = as2_clean_runtime_id(s, 32); json_free(s);
        } else ml = as2_clean_runtime_id(use, 32);
        json_set(result, "model_lock", json_string(ml));
        free(ml);
    }
    free(provider); free(model); free(source);
    json_free(payload);
    return result;
}

/* ── state mirror access (borrowed) ───────────────────────────────────────── */
static json_t *as2_st_obj(as2_env *e, const char *key) {
    json_t *v = json_obj_get(e->state, key);
    return as2_is_obj(v) ? v : NULL;
}
static json_t *as2_st_arr(as2_env *e, const char *key) {
    json_t *v = json_obj_get(e->state, key);
    return as2_is_arr(v) ? v : NULL;
}
static double as2_st_num(as2_env *e, const char *key, double def) {
    return json_get_num(e->state, key, def);
}
static const char *as2_st_str(as2_env *e, const char *key, const char *def) {
    return json_get_str(e->state, key, def);
}
static json_t *as2_runtime_kwargs(as2_env *e) { return as2_st_obj(e, "runtime_kwargs"); }

/* sorted numeric array copy (for _latency_p95_ms). */
static int as2_dbl_cmp(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

/* ── metrics/status cluster ───────────────────────────────────────────────── */

/* PoP @ gateway/platforms/api_server.py:mark_shutdown_requested */
static json_t *as2_mark_shutdown_requested(as2_env *e, const json_t *in) {
    (void)in;
    return as2_seam_obj(e, "runs._mark_shutdown_requested");
}

/* PoP @ gateway/platforms/api_server.py:_active_structured_run_count */
static json_t *as2_active_structured_run_count(as2_env *e, const json_t *in) {
    (void)in;
    json_t *statuses = as2_st_obj(e, "run_statuses");
    long count = 0;
    if (statuses) {
        for (size_t i = 0; i < statuses->c.count; i++) {
            const json_t *rec = statuses->c.items[i];
            const char *st = as2_is_obj(rec) ? json_get_str(rec, "status", "") : "";
            if (strcmp(st,"queued")==0||strcmp(st,"running")==0||
                strcmp(st,"waiting_for_approval")==0||strcmp(st,"stopping")==0)
                count++;
        }
    }
    return json_number((double)count);
}

/* PoP @ gateway/platforms/api_server.py:_resolve_api_server_int */
static json_t *as2_resolve_api_server_int(as2_env *e, const json_t *in) {
    const char *key = json_get_str(in, "key", "");
    double def = json_get_num(in, "default", 0);
    /* try: value = int(cfg_get(load_config(), "gateway","api_server",key,default=default)) */
    json_t *cfg = as2_seam_obj(e, "config.load_config");
    if (as2_is_raise(cfg)) { if (as2_swallow_exc(cfg)) return cfg; return json_number(def); }
    json_t *a = json_object();
    json_set(a, "config", cfg ? cfg : json_null());
    json_set(a, "key", json_string(key));
    json_set(a, "default", json_number(def));
    json_t *val = as2_call(e, "config.cfg_get", a);
    if (as2_is_raise(val)) { if (as2_swallow_exc(val)) return val; val = NULL; }
    double iv = 0;
    json_t *err = as2_py_int(val, &iv);
    json_free(val);
    if (err) { if (as2_swallow_exc(err)) return err; return json_number(def); }
    return json_number(iv < 0 ? 0 : iv);
}

/* PoP @ gateway/platforms/api_server.py:_metrics_day_key */
static json_t *as2_metrics_day_key(as2_env *e, const json_t *in) {
    const json_t *ts = json_obj_get(in, "timestamp");
    double t;
    if (as2_truth(ts)) t = ts->num_val;
    else {
        json_t *now = as2_seam_obj(e, "time.time");
        if (as2_is_raise(now)) return now;
        t = now ? now->num_val : 0.0;
        json_free(now);
    }
    char *dk = as2_day_key(t);
    json_t *r = json_string(dk);
    free(dk);
    return r;
}

/* PoP @ gateway/platforms/api_server.py:_iso_timestamp */
static json_t *as2_iso_timestamp(as2_env *e, const json_t *in) {
    (void)e;
    const json_t *ts = json_obj_get(in, "timestamp");
    if (as2_is_null(ts)) return json_null();
    char *s = as2_iso_format(ts->num_val);
    json_t *r = json_string(s);
    free(s);
    return r;
}

/* PoP @ gateway/platforms/api_server.py:_reset_metrics_if_needed */
static json_t *as2_reset_metrics_if_needed(as2_env *e, const json_t *in) {
    (void)in;
    json_t *dk_in = json_object();   /* _metrics_day_key() with no timestamp */
    json_t *cur = as2_metrics_day_key(e, dk_in);
    json_free(dk_in);
    if (as2_is_raise(cur)) return cur;
    const char *current_day = as2_sval(cur);
    const char *have = as2_st_str(e, "metrics_day", "");
    if (strcmp(have, current_day) == 0) { json_free(cur); return json_null(); }
    json_set(e->state, "metrics_day", json_string(current_day));
    json_set(e->state, "metrics_requests_today", json_number(0));
    json_set(e->state, "metrics_messages_today", json_number(0));
    json_set(e->state, "metrics_tokens_today", json_number(0));
    json_t *lat = as2_st_arr(e, "metrics_latency_ms");
    if (lat) { for (size_t i = 0; i < lat->c.count; i++) json_free(lat->c.items[i]); lat->c.count = 0; }
    json_free(cur);
    return json_null();
}

/* PoP @ gateway/platforms/api_server.py:_total_tokens_from_usage */
static json_t *as2_total_tokens_from_usage(as2_env *e, const json_t *in) {
    (void)e;
    const json_t *usage = json_obj_get(in, "usage");
    if (!as2_is_obj(usage)) return json_number(0);
    const json_t *tt = json_obj_get(usage, "total_tokens");
    const json_t *operand = as2_truth(tt) ? tt : NULL;   /* total_tokens or 0 */
    double iv = 0;
    if (operand) {
        json_t *err = as2_py_int(operand, &iv);
        if (err) {
            const char *k = as2_kind(err);
            if (as2_exc_is(k,"TypeError")||as2_exc_is(k,"ValueError")) { json_free(err); return json_number(0); }
            return err;
        }
    }
    return json_number(iv < 0 ? 0 : iv);
}

/* PoP @ gateway/platforms/api_server.py:_latency_p95_ms */
static json_t *as2_latency_p95_ms(as2_env *e, const json_t *in) {
    (void)e;
    const json_t *samples = json_obj_get(in, "samples");
    if (!as2_truth(samples)) return json_null();
    size_t n = samples->c.count;
    double *vals = malloc(n * sizeof(double));
    for (size_t i = 0; i < n; i++) {
        const json_t *it = samples->c.items[i];
        vals[i] = as2_is_num(it) ? it->num_val : 0.0;
    }
    qsort(vals, n, sizeof(double), as2_dbl_cmp);
    long idx = (long)(n * 0.95 + 0.999999) - 1;
    if (idx < 0) idx = 0;
    if (idx > (long)n - 1) idx = (long)n - 1;
    double r = as2_round2(vals[idx]);
    free(vals);
    return json_number(r);
}

/* PoP @ gateway/platforms/api_server.py:_api_server_status_payload */
static json_t *as2_api_server_status_payload(as2_env *e, const json_t *in) {
    json_t *r = as2_reset_metrics_if_needed(e, in);
    if (as2_is_raise(r)) return r;
    json_free(r);
    const json_t *hb_at = json_obj_get(in, "heartbeat_at");
    const json_t *heartbeat = as2_is_null(hb_at) ? json_obj_get(e->state, "metrics_last_heartbeat_at") : hb_at;
    json_t *active = as2_active_structured_run_count(e, in);
    if (as2_is_raise(active)) return active;
    double active_runs = active->num_val + as2_st_num(e, "inflight_agent_runs", 0);
    json_free(active);
    json_t *statuses = as2_st_obj(e, "run_statuses");
    double stored = statuses ? (double)statuses->c.count : 0;
    /* last_request_at = _iso_timestamp(metrics_last_request_at) */
    json_t *lra_in = json_object();
    json_t *lra_v = json_obj_get(e->state, "metrics_last_request_at");
    json_set(lra_in, "timestamp", lra_v ? json_copy(lra_v) : json_null());
    json_t *last_request = as2_iso_timestamp(e, lra_in);
    json_free(lra_in);
    if (as2_is_raise(last_request)) return last_request;
    json_t *hb_in = json_object();
    json_set(hb_in, "timestamp", heartbeat ? json_copy(heartbeat) : json_null());
    json_t *last_hb = as2_iso_timestamp(e, hb_in);
    json_free(hb_in);
    if (as2_is_raise(last_hb)) { json_free(last_request); return last_hb; }
    json_t *lat_in = json_object();
    json_t *lat = as2_st_arr(e, "metrics_latency_ms");
    json_set(lat_in, "samples", lat ? json_copy(lat) : json_array());
    json_t *p95 = as2_latency_p95_ms(e, lat_in);
    json_free(lat_in);
    if (as2_is_raise(p95)) { json_free(last_request); json_free(last_hb); return p95; }
    json_t *metrics = json_object();
    json_set(metrics, "day", json_string(as2_st_str(e, "metrics_day", "")));
    json_set(metrics, "requests", json_number(as2_st_num(e, "metrics_requests_today", 0)));
    json_set(metrics, "messages", json_number(as2_st_num(e, "metrics_messages_today", 0)));
    json_set(metrics, "tokens", json_number(as2_st_num(e, "metrics_tokens_today", 0)));
    json_set(metrics, "latency_p95_ms", p95);
    json_t *out = json_object();
    json_set(out, "host", json_string(as2_st_str(e, "host", "127.0.0.1")));
    json_set(out, "port", json_number(as2_st_num(e, "port", AS2_DEFAULT_PORT)));
    json_set(out, "active_runs", json_number(active_runs));
    json_set(out, "stored_runs", json_number(stored));
    json_set(out, "last_request_at", last_request);
    json_set(out, "last_heartbeat", last_hb);
    json_set(out, "metrics_today", metrics);
    return out;
}

/* PoP @ gateway/platforms/api_server.py:_publish_runtime_status */
static json_t *as2_publish_runtime_status(as2_env *e, const json_t *in) {
    (void)in;
    json_t *now = as2_seam_obj(e, "time.time");
    if (as2_is_raise(now)) return now;
    json_set(e->state, "metrics_last_heartbeat_at", now ? now : json_null());
    json_t *conn = as2_seam_obj(e, "is_connected");
    if (as2_is_raise(conn)) return conn;
    const char *platform_state = as2_truth(conn) ? "connected" : "disconnected";
    json_free(conn);
    json_t *payload = as2_api_server_status_payload(e, in);
    if (as2_is_raise(payload)) return payload;
    json_t *a = json_object();
    json_set(a, "context", json_string("api_server_heartbeat"));
    json_set(a, "platform_state", json_string(platform_state));
    json_set(a, "platform_metrics", payload);
    json_t *r = as2_call(e, "self._write_runtime_status_safe", a);
    if (as2_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP @ gateway/platforms/api_server.py:_record_api_metrics */
static json_t *as2_record_api_metrics(as2_env *e, const json_t *in) {
    json_t *r = as2_reset_metrics_if_needed(e, in);
    if (as2_is_raise(r)) return r;
    json_free(r);
    json_set(e->state, "metrics_requests_today",
        json_number(as2_st_num(e,"metrics_requests_today",0) + 1));
    json_set(e->state, "metrics_messages_today",
        json_number(as2_st_num(e,"metrics_messages_today",0) + 1));
    json_t *tt_in = json_object();
    const json_t *usage = json_obj_get(in, "usage");
    json_set(tt_in, "usage", usage ? json_copy(usage) : json_null());
    json_t *tokens = as2_total_tokens_from_usage(e, tt_in);
    json_free(tt_in);
    if (as2_is_raise(tokens)) return tokens;
    json_set(e->state, "metrics_tokens_today",
        json_number(as2_st_num(e,"metrics_tokens_today",0) + tokens->num_val));
    json_free(tokens);
    json_t *now = as2_seam_obj(e, "time.time");
    if (as2_is_raise(now)) return now;
    json_set(e->state, "metrics_last_request_at", now ? now : json_null());
    double latency_seconds = json_get_num(in, "latency_seconds", 0);
    double ms = latency_seconds * 1000.0;
    if (ms < 0.0) ms = 0.0;
    json_t *lat = as2_st_arr(e, "metrics_latency_ms");
    if (!lat) { lat = json_array(); json_set(e->state, "metrics_latency_ms", lat); }
    json_append(lat, json_number(ms));
    if (lat->c.count > AS2_LATENCY_SAMPLE_LIMIT) {
        size_t drop = lat->c.count - AS2_LATENCY_SAMPLE_LIMIT;
        for (size_t i = 0; i < drop; i++) json_free(lat->c.items[i]);
        for (size_t i = drop; i < lat->c.count; i++) lat->c.items[i-drop] = lat->c.items[i];
        lat->c.count -= drop;
    }
    return as2_publish_runtime_status(e, in);
}

/* PoP @ gateway/platforms/api_server.py:_heartbeat_loop */
static json_t *as2_heartbeat_loop(as2_env *e, const json_t *in) {
    (void)in;
    for (;;) {
        json_t *conn = as2_seam_obj(e, "is_connected");
        if (as2_is_raise(conn)) return conn;
        bool connected = as2_truth(conn);
        json_free(conn);
        if (!connected) break;
        json_t *r = as2_publish_runtime_status(e, in);
        if (as2_is_raise(r)) return r;
        json_free(r);
        json_t *conn2 = as2_seam_obj(e, "is_connected");
        if (as2_is_raise(conn2)) return conn2;
        bool still = as2_truth(conn2);
        json_free(conn2);
        if (!still) break;
        json_t *a = json_object();
        json_set(a, "seconds", json_number(30.0));
        json_t *sl = as2_call(e, "asyncio.sleep", a);
        if (as2_is_raise(sl)) return sl;
        json_free(sl);
    }
    return json_null();
}

/* ── provider/model cluster ───────────────────────────────────────────────── */

/* PoP @ gateway/platforms/api_server.py:_resolve_provider_runtime */
static json_t *as2_resolve_provider_runtime(as2_env *e, const json_t *in) {
    const json_t *provider = json_obj_get(in, "provider");
    const json_t *target_model = json_obj_get(in, "target_model");
    bool required = json_get_bool(in, "required", false);
    char *provider_name = as2_clean_request_string(provider);
    if (!provider_name) return json_null();
    /* target_model or None */
    json_t *tm = as2_truth(target_model) ? json_copy(target_model) : json_null();
    json_t *a = json_object();
    json_set(a, "provider", json_string(provider_name));
    json_set(a, "target_model", tm);
    json_t *res = as2_call(e, "resolve_request_runtime_agent_kwargs", a);
    if (!as2_is_raise(res)) { free(provider_name); return res; }
    /* except Exception as exc: fallback then required-raise */
    json_t *cause = res;   /* owned raise spec */
    if (!as2_is_exception(as2_kind(cause))) { free(provider_name); return cause; }
    /* with suppress(Exception): fallback */
    json_t *fa = json_object();
    json_set(fa, "provider", json_string(provider_name));
    json_set(fa, "target_model", as2_truth(target_model) ? json_copy(target_model) : json_null());
    json_t *fallback = as2_call(e, "run._resolve_runtime_agent_kwargs_for_provider", fa);
    if (as2_is_raise(fallback)) {
        if (as2_swallow_exc(fallback)) { json_free(cause); free(provider_name); return fallback; }
        /* suppressed */
    } else {
        json_free(cause); free(provider_name);
        return fallback;
    }
    if (required) {
        json_t *r = as2_raise_spec("_ProviderAuthResolutionError", as2_msg(cause));
        json_free(cause); free(provider_name);
        return r;
    }
    /* logger.debug silent */
    json_free(cause); free(provider_name);
    return json_null();
}

/* PoP @ gateway/platforms/api_server.py:_apply_provider_runtime */
static json_t *as2_apply_provider_runtime(as2_env *e, const json_t *in) {
    json_t *pr = as2_resolve_provider_runtime(e, in);
    if (as2_is_raise(pr)) return pr;
    bool applied = as2_truth(pr);
    if (applied) {
        json_t *rk = as2_runtime_kwargs(e);
        if (rk) as2_apply_overrides(rk, pr);
    }
    json_free(pr);
    return json_bool(applied);
}

/* PoP @ gateway/platforms/api_server.py:_recover_or_record_model */
static json_t *as2_recover_or_record_model(as2_env *e, const json_t *in) {
    const char *model_in = json_get_str(in, "model", "");
    const char *gsk = json_get_str(in, "gateway_session_key", "");
    json_t *rk = as2_runtime_kwargs(e);
    char *model = strdup(model_in ? model_in : "");
    /* if not model and runtime_kwargs.get("provider"): default catalog model */
    const json_t *prov = rk ? json_obj_get(rk, "provider") : NULL;
    if (model[0] == '\0' && as2_truth(prov)) {
        json_t *a = json_object();
        json_set(a, "provider", json_copy(prov));
        json_t *dm = as2_call(e, "models.get_default_model_for_provider", a);
        if (as2_is_raise(dm)) {
            if (as2_swallow_exc(dm)) { free(model); return dm; }
        } else {
            if (as2_truth(dm)) {
                free(model);
                model = strdup(as2_sval(dm));
                /* logger.info silent */
            }
            json_free(dm);
        }
    }
    char *resolved_key = strdup(gsk ? gsk : "");
    json_t *lrm = as2_st_obj(e, "last_resolved_model");
    const char *model_name = as2_st_str(e, "model_name", "hermes-agent");
    if (model[0] == '\0') {
        const json_t *rec = lrm ? json_obj_get(lrm, resolved_key) : NULL;
        if (!as2_truth(rec)) rec = lrm ? json_obj_get(lrm, "*") : NULL;
        if (as2_truth(rec)) {
            const char *recovered = as2_sval(rec);
            if (strcmp(recovered, model_name) != 0) {
                /* logger.warning silent */
                free(model); model = strdup(recovered);
            }
        }
    } else if (strcmp(model, model_name) != 0) {
        if (!lrm) { lrm = json_object(); json_set(e->state, "last_resolved_model", lrm); }
        if (resolved_key[0]) json_set(lrm, resolved_key, json_string(model));
        json_set(lrm, "*", json_string(model));
    }
    json_t *out = json_string(model);
    free(model); free(resolved_key);
    return out;
}

/* PoP @ gateway/platforms/api_server.py:_select_agent_runtime */
static json_t *as2_select_agent_runtime(as2_env *e, const json_t *in) {
    json_t *rk = as2_runtime_kwargs(e);
    if (!rk) { rk = json_object(); json_set(e->state, "runtime_kwargs", rk); }
    const char *model_in = json_get_str(in, "model", "");
    char *model = strdup(model_in ? model_in : "");
    char *request_model = as2_clean_request_string(json_obj_get(in, "requested_model"));
    char *request_provider = as2_clean_request_string(json_obj_get(in, "requested_provider"));
    const json_t *route = json_obj_get(in, "route");
    json_t *route_cfg = as2_is_obj(route) ? json_copy(route) : json_object();
    char *route_model = as2_clean_request_string(json_obj_get(route_cfg, "model"));
    char *route_provider = as2_clean_request_string(json_obj_get(route_cfg, "provider"));
    const json_t *gsk = json_obj_get(in, "gateway_session_key");
    const json_t *sid = json_obj_get(in, "session_id");
    const char *session_key = as2_truth(gsk) ? as2_sval(gsk) : (as2_truth(sid) ? as2_sval(sid) : NULL);
    char *session_row_model = as2_clean_request_string(json_obj_get(in, "session_model"));
    char *current_provider = as2_clean_request_string(json_obj_get(rk, "provider"));
    bool confirmed_runtime_lock = json_get_bool(in, "confirmed_runtime_lock", false);
    /* session_override = None if confirmed else self._session_model_override_for(session_key) */
    json_t *session_override = NULL;
    json_t *fail_ret = NULL;
    json_t *ret_override = NULL;
    if (!confirmed_runtime_lock) {
        json_t *a = json_object();
        json_set(a, "session_key", session_key ? json_string(session_key) : json_null());
        session_override = as2_call(e, "session_model_override_for", a);
        if (as2_is_raise(session_override)) { fail_ret = session_override; session_override = NULL; goto fail; }
    }
    if (as2_truth(session_override)) {
        /* model = resolve_effective_model(session_override, None, model) */
        json_t *a = json_object();
        json_set(a, "explicit", json_copy(session_override));
        json_set(a, "profile_name", json_null());
        json_set(a, "default", json_string(model));
        json_t *em = as2_call(e, "model_switch.resolve_effective_model", a);
        if (as2_is_raise(em)) { fail_ret = em; json_free(session_override); session_override = NULL; goto fail; }
        free(model); model = strdup(as2_sval(em)); json_free(em);
        /* _apply_provider_runtime(rk, clean(override.provider) or current_provider, target_model=model) */
        char *ov_prov = as2_clean_request_string(json_obj_get(session_override, "provider"));
        const char *use_prov = ov_prov ? ov_prov : current_provider;
        json_t *ap_in = json_object();
        json_set(ap_in, "provider", use_prov ? json_string(use_prov) : json_null());
        json_set(ap_in, "target_model", json_string(model));
        json_set(ap_in, "required", json_bool(false));
        json_t *ap = as2_apply_provider_runtime(e, ap_in);
        json_free(ap_in); free(ov_prov);
        if (as2_is_raise(ap)) { fail_ret = ap; json_free(session_override); session_override = NULL; goto fail; }
        json_free(ap);
        as2_apply_overrides(rk, session_override);
        if (as2_truth(route) || request_model || request_provider) { /* logger.debug silent */ }
        ret_override = json_copy(session_override);
    } else if (session_row_model && session_row_model[0] && !confirmed_runtime_lock) {
        json_t *ap_in = json_object();
        json_set(ap_in, "provider", current_provider ? json_string(current_provider) : json_null());
        json_set(ap_in, "target_model", json_string(session_row_model));
        json_set(ap_in, "required", json_bool(false));
        json_t *ap = as2_apply_provider_runtime(e, ap_in);
        json_free(ap_in);
        if (as2_is_raise(ap)) { fail_ret = ap; json_free(session_override); session_override = NULL; goto fail; }
        json_free(ap);
        json_t *a = json_object();
        json_set(a, "explicit", json_null());
        json_set(a, "profile_name", json_string(session_row_model));
        json_set(a, "default", json_string(model));
        json_t *em = as2_call(e, "model_switch.resolve_effective_model", a);
        if (as2_is_raise(em)) { fail_ret = em; json_free(session_override); session_override = NULL; goto fail; }
        free(model); model = strdup(as2_sval(em)); json_free(em);
        if (request_model || request_provider) { /* logger.debug silent */ }
    } else {
        const char *effective_model;
        char *em_buf = NULL;
        if (as2_nonnull(route)) {
            effective_model = (route_model && route_model[0]) ? route_model : model;
        } else {
            effective_model = (request_model && request_model[0]) ? request_model : model;
        }
        const char *effective_provider =
            (request_provider && request_provider[0]) ? request_provider :
            (route_provider && route_provider[0]) ? route_provider : current_provider;
        bool applied = false;
        bool req_or_route_prov = (request_provider && request_provider[0]) || (route_provider && route_provider[0]);
        if (effective_provider && effective_provider[0] &&
            (req_or_route_prov || strcmp(effective_model, model) != 0)) {
            json_t *ap_in = json_object();
            json_set(ap_in, "provider", json_string(effective_provider));
            json_set(ap_in, "target_model", json_string(effective_model));
            json_set(ap_in, "required", json_bool((request_provider && request_provider[0]) || confirmed_runtime_lock));
            json_t *ap = as2_apply_provider_runtime(e, ap_in);
            json_free(ap_in);
            if (as2_is_raise(ap)) { fail_ret = ap; json_free(session_override); session_override = NULL; goto fail; }
            applied = as2_truth(ap);
            json_free(ap);
        }
        if (!applied && effective_provider && effective_provider[0] &&
            (!current_provider || strcmp(effective_provider, current_provider) != 0)) {
            json_set(rk, "provider", json_string(effective_provider));
        }
        em_buf = strdup(effective_model);
        free(model); model = em_buf;
        for (int ki = 0; ki < 2; ki++) {
            const char *key = ki == 0 ? "api_key" : "base_url";
            char *value = as2_clean_request_string(json_obj_get(route_cfg, key));
            if (value) { json_set(rk, key, json_string(value)); free(value); }
        }
        if (as2_truth(route)) { /* logger.debug silent */ }
    }
    /* model = self._recover_or_record_model(model, rk, gateway_session_key) */
    {
        json_t *rr_in = json_object();
        json_set(rr_in, "model", json_string(model));
        json_set(rr_in, "gateway_session_key", gsk ? json_copy(gsk) : json_null());
        json_t *rr = as2_recover_or_record_model(e, rr_in);
        json_free(rr_in);
        if (as2_is_raise(rr)) { fail_ret = rr; json_free(session_override); session_override = NULL; goto fail; }
        free(model); model = strdup(as2_sval(rr)); json_free(rr);
    }
    {
        json_t *out = json_array();
        json_append(out, json_string(model));
        json_append(out, ret_override ? ret_override : json_null());
        json_append(out, request_model ? json_string(request_model) : json_null());
        json_append(out, request_provider ? json_string(request_provider) : json_null());
        free(model); free(request_model); free(request_provider);
        free(route_model); free(route_provider); free(session_row_model); free(current_provider);
        json_free(route_cfg); json_free(session_override);
        return out;
    }
fail:
    free(model); free(request_model); free(request_provider);
    free(route_model); free(route_provider); free(session_row_model); free(current_provider);
    json_free(route_cfg); json_free(session_override); json_free(ret_override);
    return fail_ret;   /* the raise spec */
}

/* _ProviderAuthResolutionError.is_rate_limited */
/* PoP @ gateway/platforms/api_server.py:is_rate_limited */
static json_t *as2_is_rate_limited(as2_env *e, const json_t *in) {
    const json_t *error = json_obj_get(in, "error");
    const json_t *cause = error ? json_obj_get(error, "cause") : NULL;
    /* cause = getattr(cause, "__cause__", None) if isinstance(cause, RuntimeError) */
    const json_t *probe = cause;
    if (as2_is_obj(cause)) {
        const char *ck = json_get_str(cause, "kind", "");
        if (as2_exc_is(ck, "RuntimeError")) probe = json_obj_get(cause, "cause");
    }
    json_t *a = json_object();
    if (as2_is_obj(probe)) {
        json_t *p = json_object();
        json_set(p, "kind", json_string(json_get_str(probe, "kind", "")));
        json_set(p, "message", json_string(json_get_str(probe, "message", "")));
        json_set(a, "cause", p);
    } else json_set(a, "cause", json_null());
    json_t *r = as2_call(e, "auth.is_rate_limited_auth_error", a);
    if (as2_is_raise(r)) return r;
    bool b = as2_truth(r);
    json_free(r);
    return json_bool(b);
}

/* _ProviderAuthResolutionError.user_text */
/* PoP @ gateway/platforms/api_server.py:user_text */
static json_t *as2_user_text(as2_env *e, const json_t *in) {
    json_t *rl = as2_is_rate_limited(e, in);
    if (as2_is_raise(rl)) return rl;
    bool rate_limited = as2_truth(rl);
    json_free(rl);
    const char *label_key = rate_limited ? "platform.api_server.provider_rate_limited"
                                         : "platform.api_server.provider_auth_failed";
    json_t *la = json_object();
    json_set(la, "key", json_string(label_key));
    json_set(la, "kwargs", json_object());
    json_t *label = as2_call(e, "t", la);
    if (as2_is_raise(label)) return label;
    const json_t *error = json_obj_get(in, "error");
    const char *emsg = error ? json_get_str(error, "message", "") : "";
    json_t *ka = json_object();
    json_set(ka, "key", json_string("platform.api_server.provider_error_line"));
    json_t *kw = json_object();
    json_set(kw, "label", label ? label : json_null());
    json_set(kw, "error", json_string(emsg));
    json_set(ka, "kwargs", kw);
    json_t *r = as2_call(e, "t", ka);
    if (as2_is_raise(r)) { json_free(label); return r; }
    json_free(label);
    return r;
}

/* ── turn metadata cluster ────────────────────────────────────────────────── */

/* PoP @ gateway/platforms/api_server.py:_effective_turn_runtime */
static json_t *as2_effective_turn_runtime(as2_env *e, const json_t *in) {
    (void)e;
    const json_t *runtime_request = json_obj_get(in, "runtime_request");
    const json_t *result = json_obj_get(in, "result");
    const json_t *usage = json_obj_get(in, "usage");
    json_t *runtime = as2_result_runtime(result, usage);
    const json_t *requested = runtime_request ? json_obj_get(runtime_request, "requested") : NULL;
    const json_t *rs = runtime_request ? json_obj_get(runtime_request, "route_source") : NULL;
    const char *route_source = as2_truth(rs) ? as2_sval(rs) : "global";
    const char *model_lock = as2_model_lock_state(runtime_request, runtime);
    json_t *out = as2_sanitize_runtime_metadata(runtime, requested, route_source, model_lock);
    json_free(runtime);
    return out;
}

/* PoP @ gateway/platforms/api_server.py:_turn_runtime_metadata */
static json_t *as2_turn_runtime_metadata(as2_env *e, const json_t *in) {
    (void)e;
    const json_t *agent = json_obj_get(in, "agent");
    const json_t *route = json_obj_get(in, "route");
    const json_t *requested_runtime = json_obj_get(in, "requested_runtime");
    const char *route_source = json_get_str(in, "route_source", "");
    bool confirmed_runtime_lock = json_get_bool(in, "confirmed_runtime_lock", false);
    /* runtime = dict(getattr(agent, "_hermes_api_runtime", {}) or {}) */
    const json_t *har = agent ? json_obj_get(agent, "_hermes_api_runtime") : NULL;
    json_t *runtime = as2_truth(har) ? json_copy(har) : json_object();
    const json_t *raw_provider = agent ? json_obj_get(agent, "provider") : NULL;
    const json_t *raw_model = agent ? json_obj_get(agent, "model") : NULL;
    char *actual_provider = as2_is_str(raw_provider) ? as2_clean_runtime_id(raw_provider, 80) : strdup("");
    char *actual_model = as2_is_str(raw_model) ? as2_clean_runtime_id(raw_model, 200) : strdup("");
    char *resolved_provider = as2_clean_runtime_id(json_obj_get(runtime, "provider"), 80);
    /* for key, actual in ((provider, actual_provider),(model, actual_model)) */
    if (actual_provider[0]) json_set(runtime, "provider", json_string(actual_provider));
    else if (!json_has(runtime, "provider")) json_set(runtime, "provider", json_string(""));
    if (actual_model[0]) json_set(runtime, "model", json_string(actual_model));
    else if (!json_has(runtime, "model")) json_set(runtime, "model", json_string(""));
    json_t *route_o = as2_truth(route) ? json_copy(route) : json_object();
    json_t *req_o = as2_truth(requested_runtime) ? json_copy(requested_runtime) : json_object();
    if (confirmed_runtime_lock) {
        const json_t *rp = json_obj_get(route_o, "provider");
        if (!as2_truth(rp)) rp = json_obj_get(req_o, "provider");
        char *requested_provider = as2_clean_runtime_id(rp, 80);
        /* expected_provider = _clean_runtime_id(resolved_provider or requested_provider, 80) */
        const char *use = resolved_provider[0] ? resolved_provider : requested_provider;
        json_t *eps = json_string(use);
        char *expected_provider = as2_clean_runtime_id(eps, 80);
        json_free(eps);
        const json_t *em = json_obj_get(route_o, "model");
        if (!as2_truth(em)) em = json_obj_get(req_o, "model");
        char *expected_model = as2_clean_runtime_id(em, 200);
        bool mismatch = (expected_provider[0] && strcmp(actual_provider, expected_provider) != 0) ||
                        (expected_model[0] && strcmp(actual_model, expected_model) != 0);
        if (mismatch) {
            const char *rp_disp = requested_provider[0] ? requested_provider :
                                  (expected_provider[0] ? expected_provider : "<unspecified>");
            const char *em_disp = expected_model[0] ? expected_model : "<unspecified>";
            const char *ap_disp = actual_provider[0] ? actual_provider : "<unknown>";
            const char *am_disp = actual_model[0] ? actual_model : "<unknown>";
            char msg[1024];
            snprintf(msg, sizeof(msg),
                "confirmed model lock runtime mismatch: expected provider=%s model=%s; actual provider=%s model=%s",
                rp_disp, em_disp, ap_disp, am_disp);
            free(requested_provider); free(expected_provider); free(expected_model);
            free(actual_provider); free(actual_model); free(resolved_provider);
            json_free(runtime); json_free(route_o); json_free(req_o);
            return as2_raise_spec("RuntimeError", msg);
        }
        free(requested_provider); free(expected_provider); free(expected_model);
    }
    if (as2_truth(req_o)) {
        char *rm, *rp; as2_requested_ids(req_o, &rm, &rp);
        json_t *req = json_object();
        json_set(req, "provider", json_string(rp));
        json_set(req, "model", json_string(rm));
        json_set(runtime, "requested", req);
        free(rm); free(rp);
    }
    /* runtime["route_source"] = route_source or runtime.get("route_source") or "global" */
    const char *rs_final = route_source[0] ? route_source :
        (as2_truth(json_obj_get(runtime, "route_source")) ? as2_sval(json_obj_get(runtime,"route_source")) : "global");
    json_set(runtime, "route_source", json_string(rs_final));
    const char *sanitize_rs = route_source[0] ? route_source : "global";
    json_t *out = as2_sanitize_runtime_metadata(runtime, as2_truth(req_o) ? req_o : NULL,
        sanitize_rs, confirmed_runtime_lock ? "confirmed" : "");
    free(actual_provider); free(actual_model); free(resolved_provider);
    json_free(runtime); json_free(route_o); json_free(req_o);
    return out;
}

/* PoP @ gateway/platforms/api_server.py:_finish_turn_result */
static json_t *as2_finish_turn_result(as2_env *e, const json_t *in) {
    const json_t *agent = json_obj_get(in, "agent");
    const json_t *session_id = json_obj_get(in, "session_id");
    json_t *result = as2_st_obj(e, "result");
    if (!result) { result = json_object(); json_set(e->state, "result", result); }
    /* usage from agent.session_*_tokens (getattr default 0) or 0 */
    json_t *usage = json_object();
    const char *tok_keys[] = {"session_prompt_tokens","session_completion_tokens","session_total_tokens"};
    const char *out_keys[] = {"input_tokens","output_tokens","total_tokens"};
    for (int i = 0; i < 3; i++) {
        const json_t *tv = agent ? json_obj_get(agent, tok_keys[i]) : NULL;
        double v = as2_truth(tv) ? (as2_is_num(tv) ? tv->num_val : 0) : 0;
        json_set(usage, out_keys[i], json_number(v));
    }
    /* _eff_sid = getattr(agent, "session_id", session_id) */
    const json_t *eff = agent ? json_obj_get(agent, "session_id") : NULL;
    if (!eff) eff = session_id;
    if (as2_is_str(eff) && as2_sval(eff)[0])
        json_set(result, "session_id", json_copy(eff));
    bool session_rotated = as2_is_str(eff) && as2_is_str(session_id) &&
        strcmp(as2_sval(eff), as2_sval(session_id)) != 0;
    const json_t *lcp = agent ? json_obj_get(agent, "_last_compaction_in_place") : NULL;
    if (as2_truth(lcp) || session_rotated)
        json_set(result, "_compressed", json_bool(true));
    const json_t *route = json_obj_get(in, "route");
    const json_t *requested_runtime = json_obj_get(in, "requested_runtime");
    const char *route_source = json_get_str(in, "route_source", "");
    bool confirmed_runtime_lock = json_get_bool(in, "confirmed_runtime_lock", false);
    if (as2_truth(requested_runtime) || as2_truth(route) || confirmed_runtime_lock ||
        (route_source[0] && strcmp(route_source, "global") != 0)) {
        json_t *trm_in = json_object();
        json_set(trm_in, "agent", agent ? json_copy(agent) : json_null());
        json_set(trm_in, "route", route ? json_copy(route) : json_null());
        json_set(trm_in, "requested_runtime", requested_runtime ? json_copy(requested_runtime) : json_null());
        json_set(trm_in, "route_source", json_string(route_source));
        json_set(trm_in, "confirmed_runtime_lock", json_bool(confirmed_runtime_lock));
        json_t *runtime = as2_turn_runtime_metadata(e, trm_in);
        json_free(trm_in);
        if (as2_is_raise(runtime)) { json_free(usage); return runtime; }
        if (as2_is_obj(result)) json_set(result, "runtime", json_copy(runtime));
        json_set(usage, "runtime", runtime);
    }
    json_t *out = json_array();
    json_append(out, json_copy(result));
    json_append(out, usage);
    return out;
}

/* ── HTTP handler cluster ─────────────────────────────────────────────────── */

/* PoP @ gateway/platforms/api_server.py:_handle_health */
static json_t *as2_handle_health(as2_env *e, const json_t *in) {
    (void)in;
    json_t *ver = as2_seam_obj(e, "hermes_version");
    if (as2_is_raise(ver)) return ver;
    json_t *data = json_object();
    json_set(data, "status", json_string("ok"));
    json_set(data, "platform", json_string("hermes-agent"));
    json_set(data, "version", ver ? ver : json_null());
    json_t *a = json_object();
    json_set(a, "data", data);
    json_set(a, "status", json_null());
    json_set(a, "headers", json_null());
    return as2_call(e, "web.json_response", a);
}

/* PoP @ gateway/platforms/api_server.py:_handle_models */
static json_t *as2_handle_models(as2_env *e, const json_t *in) {
    (void)in;
    json_t *now = as2_seam_obj(e, "time.time");
    if (as2_is_raise(now)) return now;
    double now_i = trunc(now ? now->num_val : 0.0);
    json_free(now);
    json_t *prof = as2_seam_obj(e, "request_profile");
    if (as2_is_raise(prof)) return prof;
    bool has_profile = as2_truth(prof);
    json_free(prof);
    char *model_name;
    if (has_profile) {
        json_t *a = json_object();
        json_set(a, "explicit", json_string(""));
        json_t *rm = as2_call(e, "resolve_model_name", a);
        if (as2_is_raise(rm)) return rm;
        model_name = strdup(as2_sval(rm)); json_free(rm);
    } else model_name = strdup(as2_st_str(e, "model_name", "hermes-agent"));
    json_t *models = json_array();
    /* _model(model_name, model_name, None) */
    {
        json_t *m = json_object();
        json_set(m, "id", json_string(model_name));
        json_set(m, "object", json_string("model"));
        json_set(m, "created", json_number(now_i));
        json_set(m, "owned_by", json_string("hermes"));
        json_set(m, "permission", json_array());
        json_set(m, "root", json_string(model_name));
        json_set(m, "parent", json_null());
        json_append(models, m);
    }
    json_t *routes = as2_st_obj(e, "model_routes");
    if (routes) {
        for (size_t i = 0; i < routes->c.count; i++) {
            const char *alias = routes->c.keys[i];
            if (strcmp(alias, model_name) == 0) continue;
            const json_t *rc = routes->c.items[i];
            const json_t *rm = as2_is_obj(rc) ? json_obj_get(rc, "model") : NULL;
            const char *root = as2_truth(rm) ? as2_sval(rm) : alias;
            json_t *m = json_object();
            json_set(m, "id", json_string(alias));
            json_set(m, "object", json_string("model"));
            json_set(m, "created", json_number(now_i));
            json_set(m, "owned_by", json_string("hermes"));
            json_set(m, "permission", json_array());
            json_set(m, "root", json_string(root));
            json_set(m, "parent", json_string(model_name));
            json_append(models, m);
        }
    }
    json_t *data = json_object();
    json_set(data, "object", json_string("list"));
    json_set(data, "data", models);
    free(model_name);
    json_t *a = json_object();
    json_set(a, "data", data);
    json_set(a, "status", json_null());
    json_set(a, "headers", json_null());
    return as2_call(e, "web.json_response", a);
}

/* PoP @ gateway/platforms/api_server.py:_handle_capabilities */
static json_t *as2_handle_capabilities(as2_env *e, const json_t *in) {
    (void)in;
    const json_t *runner = json_obj_get(e->state, "gateway_runner");
    /* runs_idempotency = _api_runs._idempotency_capabilities(self, store_type=RunIdempotencyStore) */
    json_t *ia = json_object();
    json_set(ia, "store_type", json_string("RunIdempotencyStore"));
    json_t *idem = as2_call(e, "runs._idempotency_capabilities", ia);
    if (as2_is_raise(idem)) return idem;
    const json_t *sa = runner ? json_obj_get(runner, "session_authority") : NULL;
    bool run_unknown = as2_nonnull(sa);
    json_t *cors = as2_st_arr(e, "cors_origins");
    bool cors_b = as2_truth(cors);
    json_t *features = json_object();
    json_set(features, "chat_completions", json_bool(true));
    json_set(features, "chat_completions_streaming", json_bool(true));
    json_set(features, "responses_api", json_bool(true));
    json_set(features, "responses_streaming", json_bool(true));
    json_set(features, "run_submission", json_bool(true));
    json_set(features, "runs_idempotency", idem ? idem : json_null());
    for (size_t i = 0; i < sizeof(AS2_STATIC_FEATURE_FLAGS)/sizeof(as2_flag); i++) {
        const as2_flag *f = &AS2_STATIC_FEATURE_FLAGS[i];
        json_set(features, f->k, f->kind == 0 ? json_bool(f->b) : json_string(f->s));
    }
    json_set(features, "run_unknown_resolution", json_bool(run_unknown));
    json_set(features, "cors", json_bool(cors_b));
    /* endpoints filtered by feature truthiness */
    json_t *endpoints = json_object();
    for (size_t i = 0; i < sizeof(AS2_CAPABILITY_ENDPOINTS)/sizeof(as2_endpoint); i++) {
        const as2_endpoint *ep = &AS2_CAPABILITY_ENDPOINTS[i];
        if (ep->feature) {
            const json_t *fv = json_obj_get(features, ep->feature);
            if (!as2_truth(fv)) continue;
        }
        json_t *entry = json_object();
        json_set(entry, "method", json_string(ep->method));
        json_set(entry, "path", json_string(ep->path));
        json_set(endpoints, ep->name, entry);
    }
    json_t *auth = json_object();
    json_set(auth, "type", json_string("bearer"));
    const char *api_key = as2_st_str(e, "api_key", "");
    json_set(auth, "required", json_bool(api_key[0] != '\0'));
    json_t *runtime = json_object();
    json_set(runtime, "mode", json_string("server_agent"));
    json_set(runtime, "tool_execution", json_string("server"));
    json_set(runtime, "split_runtime", json_bool(false));
    json_set(runtime, "description", json_string(
        "The API server creates a server-side Hermes AIAgent; "
        "tools execute on the API-server host unless a future "
        "explicit split-runtime mode is enabled."));
    json_t *data = json_object();
    json_set(data, "object", json_string("hermes.api_server.capabilities"));
    json_set(data, "platform", json_string("hermes-agent"));
    json_set(data, "model", json_string(as2_st_str(e, "model_name", "hermes-agent")));
    json_set(data, "auth", auth);
    json_set(data, "runtime", runtime);
    json_set(data, "features", features);
    json_set(data, "endpoints", endpoints);
    json_t *a = json_object();
    json_set(a, "data", data);
    json_set(a, "status", json_null());
    json_set(a, "headers", json_null());
    return as2_call(e, "web.json_response", a);
}

/* ── dispatch table ───────────────────────────────────────────────────────── */
typedef json_t *(*as2_fn)(as2_env *, const json_t *);
static const struct { const char *name; as2_fn fn; } as2_table[] = {
    {"mark_shutdown_requested", as2_mark_shutdown_requested},
    {"_active_structured_run_count", as2_active_structured_run_count},
    {"_resolve_api_server_int", as2_resolve_api_server_int},
    {"_metrics_day_key", as2_metrics_day_key},
    {"_iso_timestamp", as2_iso_timestamp},
    {"_reset_metrics_if_needed", as2_reset_metrics_if_needed},
    {"_total_tokens_from_usage", as2_total_tokens_from_usage},
    {"_latency_p95_ms", as2_latency_p95_ms},
    {"_api_server_status_payload", as2_api_server_status_payload},
    {"_publish_runtime_status", as2_publish_runtime_status},
    {"_record_api_metrics", as2_record_api_metrics},
    {"_heartbeat_loop", as2_heartbeat_loop},
    {"_resolve_provider_runtime", as2_resolve_provider_runtime},
    {"_apply_provider_runtime", as2_apply_provider_runtime},
    {"_recover_or_record_model", as2_recover_or_record_model},
    {"_select_agent_runtime", as2_select_agent_runtime},
    {"is_rate_limited", as2_is_rate_limited},
    {"user_text", as2_user_text},
    {"_effective_turn_runtime", as2_effective_turn_runtime},
    {"_turn_runtime_metadata", as2_turn_runtime_metadata},
    {"_finish_turn_result", as2_finish_turn_result},
    {"_handle_health", as2_handle_health},
    {"_handle_models", as2_handle_models},
    {"_handle_capabilities", as2_handle_capabilities},
};