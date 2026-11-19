/*
 * t_port_api_server_flow.c — oracle harness for the api_server.py request FLOW
 * cores ported in src/gateway/platforms/api_server_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * Wires each flow's seams to the SCRIPTED results in "in" (recording stubs
 * that append one trace event per seam call, in call order) and emits
 * {"func": ..., "in": ..., "out": {"ret": ..., "trace": [...], ...}} per case.
 * tests/oracle_api_server_flow.py drives the LIVE Python with the same scripts.
 *
 * Scripted seam results live under "in" keys documented per stub below; the
 * oracle's recording monkeypatches read the identical keys.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "hermes_json.h"
#include "api_server_flow.h"

typedef struct {
    const json_t *in;
    apisf_trace_t *trace;
    json_t *qstate;      /* queue double for put_nowait / sleep_zero */
    size_t clock_idx;
    size_t clean_idx;
    size_t flush_idx;
} script_ctx_t;

/* ── event builders ── */
static json_t *ev(const char *name) {
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    return e;
}
static void evs(json_t *e, const char *k, const char *v) {
    json_set(e, k, v ? json_string(v) : json_null());
}
static void evb(json_t *e, const char *k, bool v) { json_set(e, k, json_bool(v)); }
static void evn(json_t *e, const char *k, double v) { json_set(e, k, json_number(v)); }
static void evj(json_t *e, const char *k, const json_t *v) {
    json_set(e, k, v ? json_copy(v) : json_null());
}
static void rec(script_ctx_t *c, json_t *e) { apisf_trace_add(c->trace, e); }

static const json_t *in_obj(const script_ctx_t *c, const char *k) {
    const json_t *v = json_obj_get(c->in, k);
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}
static const json_t *in_arr(const script_ctx_t *c, const char *k) {
    const json_t *v = json_obj_get(c->in, k);
    return (v && v->type == JSON_ARRAY) ? v : NULL;
}
static bool in_bool(const script_ctx_t *c, const char *k) {
    return json_get_bool(c->in, k, false);
}
static const char *in_str(const script_ctx_t *c, const char *k, const char *def) {
    return json_get_str(c->in, k, def);
}
/* Scripted number sequence: array consumed in call order (last value
 * repeats); a bare number is constant. */
static double next_num(script_ctx_t *c, const char *k, size_t *idx, double def) {
    const json_t *v = json_obj_get(c->in, k);
    if (!v) return def;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_ARRAY && v->c.count > 0) {
        size_t i = *idx;
        if (i >= v->c.count) i = v->c.count - 1;
        (*idx)++;
        const json_t *item = v->c.items[i];
        return (item && item->type == JSON_NUMBER) ? item->num_val : def;
    }
    return def;
}

/* web.Response descriptor double (both sides build the identical shape). */
static json_t *response_desc(const json_t *body, int status, const json_t *headers) {
    json_t *d = json_object();
    json_set(d, "status", json_number(status));
    json_set(d, "body", body ? json_copy(body) : json_null());
    json_set(d, "headers", headers ? json_copy(headers) : json_null());
    return d;
}

/* ── seams ── */

static json_t *sf_json_response(void *ctx, const json_t *body, int status, const json_t *headers) {
    script_ctx_t *c = ctx;
    json_t *e = ev("json_response");
    evj(e, "body", body);
    evn(e, "status", status);
    evj(e, "headers", headers);
    rec(c, e);
    return response_desc(body, status, headers);
}
static char *sf_redact(void *ctx, const char *text) {
    script_ctx_t *c = ctx;
    const char *r = json_get_str(c->in, "redact_ret", text ? text : "");
    json_t *e = ev("redact");
    evs(e, "text", text);
    evs(e, "ret", r);
    rec(c, e);
    return strdup(r ? r : "");
}
static void sf_log(void *ctx, const char *level, const char *msg) {
    script_ctx_t *c = ctx;
    json_t *e = ev("log");
    evs(e, "level", level);
    evs(e, "msg", msg);
    rec(c, e);
}
static double sf_clock(void *ctx) {
    script_ctx_t *c = ctx;
    double r = next_num(c, "clock", &c->clock_idx, 0.0);
    json_t *e = ev("clock");
    evn(e, "ret", r);
    rec(c, e);
    return r;
}
static bool sf_secrets_match(void *ctx, const char *a, const char *b) {
    script_ctx_t *c = ctx;
    bool r = in_bool(c, "secrets_match_ret");
    json_t *e = ev("secrets_match");
    evs(e, "a", a);
    evs(e, "b", b);
    evb(e, "ret", r);
    rec(c, e);
    return r;
}
static char *sf_audit_suffix(void *ctx, const json_t *request) {
    script_ctx_t *c = ctx;
    (void)request;
    const char *r = in_str(c, "audit_suffix", "source='unknown'");
    json_t *e = ev("audit_suffix");
    evs(e, "ret", r);
    rec(c, e);
    return strdup(r);
}
static const char *sf_request_profile(void *ctx) {
    script_ctx_t *c = ctx;
    const json_t *p = json_obj_get(c->in, "profile");
    const char *r = (p && p->type == JSON_STRING) ? p->str_val : NULL;
    json_t *e = ev("request_profile");
    evs(e, "ret", r);
    rec(c, e);
    return r;
}
static char *sf_get_secret(void *ctx, const char *name, const char *def,
                           bool *raised, const char **exc_type) {
    script_ctx_t *c = ctx;
    *raised = in_bool(c, "secret_raises");
    *exc_type = in_str(c, "secret_exc_type", "RuntimeError");
    const json_t *rv = json_obj_get(c->in, "secret_ret");
    char *r = (rv && rv->type == JSON_STRING) ? strdup(rv->str_val) : NULL;
    json_t *e = ev("get_secret");
    evs(e, "name", name);
    evs(e, "default", def);
    if (*raised) evs(e, "raised", *exc_type);
    else evs(e, "ret", r);
    rec(c, e);
    return r;
}
static bool sf_has_usable_secret(void *ctx, const char *value, int min_length,
                                 bool *raised, const char **exc_type) {
    script_ctx_t *c = ctx;
    *raised = in_bool(c, "usable_raises");
    *exc_type = in_str(c, "usable_exc_type", "RuntimeError");
    bool r = in_bool(c, "usable_ret");
    json_t *e = ev("has_usable_secret");
    evs(e, "value", value);
    evn(e, "min_length", min_length);
    if (*raised) evs(e, "raised", *exc_type);
    else evb(e, "ret", r);
    rec(c, e);
    return r;
}
static json_t *sf_check_auth(void *ctx, const json_t *request) {
    script_ctx_t *c = ctx;
    (void)request;
    const json_t *r = json_obj_get(c->in, "check_auth_ret");
    json_t *e = ev("check_auth");
    evj(e, "ret", r);
    rec(c, e);
    return r ? json_copy(r) : NULL;
}
static json_t *sf_draining(void *ctx) {
    script_ctx_t *c = ctx;
    const json_t *r = json_obj_get(c->in, "draining_ret");
    json_t *e = ev("draining_response");
    evj(e, "ret", r);
    rec(c, e);
    return r ? json_copy(r) : NULL;
}
static json_t *sf_handler(void *ctx, const json_t *request) {
    script_ctx_t *c = ctx;
    (void)request;
    const json_t *r = json_obj_get(c->in, "handler_ret");
    json_t *e = ev("handler");
    evj(e, "ret", r);
    rec(c, e);
    return r ? json_copy(r) : json_null();
}
static char *sf_scan(void *ctx, const char *prompt) {
    script_ctx_t *c = ctx;
    const json_t *rv = json_obj_get(c->in, "scan_ret");
    char *r = (rv && rv->type == JSON_STRING) ? strdup(rv->str_val) : NULL;
    json_t *e = ev("scan_cron_prompt");
    evs(e, "prompt", prompt);
    evs(e, "ret", r);
    rec(c, e);
    return r;
}
static json_t *sf_cors(void *ctx, const char *origin) {
    script_ctx_t *c = ctx;
    const json_t *r = in_obj(c, "cors_ret");
    json_t *e = ev("cors_headers");
    evs(e, "origin", origin);
    evj(e, "ret", r);
    rec(c, e);
    return r ? json_copy(r) : NULL;
}
static char *sf_redact_approval(void *ctx, const char *text) {
    script_ctx_t *c = ctx;
    const json_t *map = in_obj(c, "approval_redactions");
    const char *r = text ? text : "";
    if (map) {
        const json_t *v = json_obj_get(map, r);
        if (v && v->type == JSON_STRING) r = v->str_val;
    }
    json_t *e = ev("redact_approval_command");
    evs(e, "text", text);
    evs(e, "ret", r);
    rec(c, e);
    return strdup(r);
}
static json_t *sf_run_event(void *ctx, const char *run_id, const char *kind, const json_t *fields) {
    script_ctx_t *c = ctx;
    const json_t *r = in_obj(c, "run_event_ret");
    json_t *e = ev("run_event");
    evs(e, "run_id", run_id);
    evs(e, "kind", kind);
    evj(e, "fields", fields);
    evj(e, "ret", r);
    rec(c, e);
    return r ? json_copy(r) : json_object();
}
static json_t *sf_choices(void *ctx, bool smart_denied, bool allow_session, bool allow_permanent) {
    script_ctx_t *c = ctx;
    const json_t *r = in_arr(c, "choices_ret");
    json_t *e = ev("approval_choices");
    evb(e, "smart_denied", smart_denied);
    evb(e, "allow_session", allow_session);
    evb(e, "allow_permanent", allow_permanent);
    evj(e, "ret", r);
    rec(c, e);
    return r ? json_copy(r) : json_array();
}
static char *sf_routing_home(void *ctx, bool *raised) {
    script_ctx_t *c = ctx;
    *raised = in_bool(c, "routing_home_raises");
    const char *r = in_str(c, "routing_home", "");
    json_t *e = ev("routing_home");
    if (*raised) evb(e, "raised", true);
    else evs(e, "ret", r);
    rec(c, e);
    return *raised ? NULL : strdup(r);
}
static bool sf_matches(void *ctx, const char *profile, const char *home, bool *raised) {
    script_ctx_t *c = ctx;
    *raised = in_bool(c, "matches_raises");
    bool r = in_bool(c, "matches_ret");
    json_t *e = ev("profile_matches_home");
    evs(e, "profile", profile);
    evs(e, "home", home);
    if (*raised) evb(e, "raised", true);
    else evb(e, "ret", r);
    rec(c, e);
    return r;
}
static json_t *sf_turn_author(void *ctx, const json_t *raw, bool *raised, const char **exc_msg) {
    script_ctx_t *c = ctx;
    *raised = in_bool(c, "turn_author_raises");
    *exc_msg = in_str(c, "turn_author_exc", "author must be an object");
    const json_t *r = in_obj(c, "turn_author_ret");
    json_t *e = ev("parse_turn_author");
    evj(e, "raw", raw);
    if (*raised) evs(e, "raised", *exc_msg);
    else evj(e, "ret", r);
    rec(c, e);
    return (*raised || !r) ? NULL : json_copy(r);
}
static char *sf_clean_id(void *ctx, const json_t *value, int max_len) {
    script_ctx_t *c = ctx;
    const json_t *rets = in_arr(c, "clean_rets");
    const char *r = "";
    if (rets && rets->c.count > 0) {
        size_t i = c->clean_idx;
        if (i >= rets->c.count) i = rets->c.count - 1;
        c->clean_idx++;
        const json_t *item = rets->c.items[i];
        if (item && item->type == JSON_STRING) r = item->str_val;
    }
    json_t *e = ev("clean_runtime_id");
    evj(e, "value", value);
    evn(e, "max_len", max_len);
    evs(e, "ret", r);
    rec(c, e);
    return strdup(r);
}
static const char *sf_running_loop(void *ctx, bool *raised) {
    script_ctx_t *c = ctx;
    *raised = in_bool(c, "running_loop_raises");
    const json_t *t = json_obj_get(c->in, "running_loop");
    const char *r = (t && t->type == JSON_STRING) ? t->str_val : NULL;
    json_t *e = ev("get_running_loop");
    if (*raised) evb(e, "raised", true);
    else evs(e, "ret", r);
    rec(c, e);
    return *raised ? NULL : r;
}
static void sf_put_nowait(void *ctx, const json_t *event) {
    script_ctx_t *c = ctx;
    json_t *e = ev("put_nowait");
    evj(e, "event", event);
    rec(c, e);
    /* queue double: the delivered events accumulate in qstate["queue"]. */
    if (c->qstate) {
        json_t *q = json_obj_get(c->qstate, "queue");
        if (!q || q->type != JSON_ARRAY) {
            q = json_array();
            json_set(c->qstate, "queue", q);
        }
        json_append(q, json_copy(event));
    }
}
static void sf_call_soon(void *ctx, void (*cb)(void *), void *cb_ctx, bool *raised) {
    script_ctx_t *c = ctx;
    *raised = in_bool(c, "cs_raises");
    json_t *e = ev("call_soon_threadsafe");
    evb(e, "raised", *raised);
    rec(c, e);
    /* Single-threaded scripted replay: run the callback synchronously. */
    if (!*raised && cb) cb(cb_ctx);
}
static void sf_sleep_zero(void *ctx) {
    script_ctx_t *c = ctx;
    json_t *e = ev("sleep_zero");
    rec(c, e);
    /* Scripted cross-thread settles: each tick applies the next inflight
     * value (0 once the script is exhausted, so flush terminates). */
    if (c->qstate) {
        const json_t *script = in_arr(c, "flush_inflight");
        double v = 0;
        if (script && script->c.count > 0) {
            size_t i = c->flush_idx;
            if (i >= script->c.count) i = script->c.count - 1;
            c->flush_idx++;
            const json_t *item = script->c.items[i];
            if (item && item->type == JSON_NUMBER) v = item->num_val;
        }
        json_set(c->qstate, "inflight", json_number(v));
    }
}
static void sf_set_run_status(void *ctx, const char *run_id, const char *status,
                              const char *last_event, const json_t *approval) {
    script_ctx_t *c = ctx;
    json_t *e = ev("set_run_status");
    evs(e, "run_id", run_id);
    evs(e, "status", status);
    evs(e, "last_event", last_event);
    evj(e, "approval", approval);
    rec(c, e);
}
static void sf_enqueue(void *ctx, const char *name, const json_t *payload) {
    script_ctx_t *c = ctx;
    json_t *e = ev("enqueue");
    evs(e, "name", name);
    evj(e, "payload", payload);
    rec(c, e);
}

static apisf_seams_t wired_seams(script_ctx_t *c) {
    apisf_seams_t s = {
        .ctx = c,
        .json_response = sf_json_response,
        .redact = sf_redact,
        .log = sf_log,
        .clock = sf_clock,
        .secrets_match = sf_secrets_match,
        .audit_log_suffix = sf_audit_suffix,
        .request_profile = sf_request_profile,
        .get_secret = sf_get_secret,
        .has_usable_secret = sf_has_usable_secret,
        .check_auth = sf_check_auth,
        .draining_response = sf_draining,
        .handler = sf_handler,
        .scan_cron_prompt = sf_scan,
        .cors_headers_for_origin = sf_cors,
        .redact_approval_command = sf_redact_approval,
        .run_event = sf_run_event,
        .approval_choices = sf_choices,
        .routing_process_home = sf_routing_home,
        .profile_matches_home = sf_matches,
        .parse_turn_author = sf_turn_author,
        .clean_runtime_id = sf_clean_id,
        .get_running_loop = sf_running_loop,
        .put_nowait = sf_put_nowait,
        .call_soon_threadsafe = sf_call_soon,
        .sleep_zero = sf_sleep_zero,
        .set_run_status = sf_set_run_status,
        .enqueue = sf_enqueue,
    };
    return s;
}

/* ── dispatch ── */
static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = { in, NULL, NULL, 0, 0, 0 };
    apisf_trace_t trace = { NULL };
    ctx.trace = &trace;
    apisf_seams_t s = wired_seams(&ctx);
    const json_t *request = json_obj_get(in, "request");

    if (strcmp(func, "cap_text") == 0) {
        char *r = apisf_cap_text(in_str(&ctx, "text", ""));
        json_set(out, "ret", json_string(r ? r : ""));
        free(r);
    } else if (strcmp(func, "cap_list") == 0) {
        json_t *r = apisf_cap_list(json_obj_get(in, "items"));
        json_set(out, "ret", r);
    } else if (strcmp(func, "coerce_request_bool") == 0) {
        bool r = apisf_coerce_request_bool(json_obj_get(in, "value"), in_bool(&ctx, "default"));
        json_set(out, "ret", json_bool(r));
    } else if (strcmp(func, "request_relay_metadata") == 0) {
        json_t *r = apisf_request_relay_metadata(json_obj_get(in, "body"));
        json_set(out, "ret", r);
    } else if (strcmp(func, "normalize_image_part") == 0) {
        char *err = NULL;
        json_t *r = apisf_normalize_image_part(json_obj_get(in, "part"), &err);
        json_set(out, "ret", r ? r : json_null());
        json_set(out, "err", err ? json_string(err) : json_null());
        free(err);
    } else if (strcmp(func, "chat_usage_payload") == 0) {
        json_set(out, "ret", apisf_chat_usage_payload(json_obj_get(in, "usage")));
    } else if (strcmp(func, "responses_usage_payload") == 0) {
        json_set(out, "ret", apisf_responses_usage_payload(json_obj_get(in, "usage")));
    } else if (strcmp(func, "make_request_fingerprint") == 0) {
        char *r = apisf_make_request_fingerprint(json_obj_get(in, "body"), json_obj_get(in, "keys"));
        json_set(out, "ret", json_string(r ? r : ""));
        free(r);
    } else if (strcmp(func, "session_headers") == 0) {
        const json_t *k = json_obj_get(in, "gateway_session_key");
        json_set(out, "ret", apisf_session_headers(
            in_str(&ctx, "session_id", ""),
            (k && k->type == JSON_STRING) ? k->str_val : NULL));
    } else if (strcmp(func, "result_runtime") == 0) {
        json_set(out, "ret", apisf_result_runtime(json_obj_get(in, "result"), json_obj_get(in, "usage")));
    } else if (strcmp(func, "model_lock_state") == 0) {
        const char *r = apisf_model_lock_state(json_obj_get(in, "runtime_request"), json_obj_get(in, "runtime"));
        json_set(out, "ret", json_string(r));
    } else if (strcmp(func, "requested_ids") == 0) {
        json_set(out, "ret", apisf_requested_ids(json_obj_get(in, "requested"), &s));
    } else if (strcmp(func, "names_launch_profile") == 0) {
        bool r = apisf_names_launch_profile(in_str(&ctx, "profile_name", ""), &s);
        json_set(out, "ret", json_bool(r));
    } else if (strcmp(func, "derive_chat_session_id") == 0) {
        const json_t *sp = json_obj_get(in, "system_prompt");
        const json_t *pf = json_obj_get(in, "profile");
        char *r = apisf_derive_chat_session_id(
            (sp && sp->type == JSON_STRING) ? sp->str_val : NULL,
            in_str(&ctx, "first_user_message", ""),
            (pf && pf->type == JSON_STRING) ? pf->str_val : NULL, &s);
        json_set(out, "ret", json_string(r ? r : ""));
        free(r);
    } else if (strcmp(func, "request_turn_author") == 0) {
        char *err = NULL;
        json_t *r = apisf_request_turn_author(json_obj_get(in, "body"), &s, &err);
        json_set(out, "ret", r ? r : json_null());
        json_set(out, "err", err ? json_string(err) : json_null());
        free(err);
    } else if (strcmp(func, "error_response") == 0) {
        const json_t *p = json_obj_get(in, "param");
        const json_t *cd = json_obj_get(in, "code");
        json_t *r = apisf_error_response(
            in_str(&ctx, "message", ""), (int)json_get_num(in, "status", 400),
            in_str(&ctx, "err_type", "invalid_request_error"),
            (p && p->type == JSON_STRING) ? p->str_val : NULL,
            (cd && cd->type == JSON_STRING) ? cd->str_val : NULL,
            json_obj_get(in, "headers"), &s);
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "invalid_request") == 0) {
        json_t *r = apisf_invalid_request(in_str(&ctx, "message", ""), &s);
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "auth_failed_response") == 0) {
        json_t *r = apisf_auth_failed_response(&s);
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "session_db_unavailable") == 0) {
        json_t *r = apisf_session_db_unavailable(&s);
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "cron_error_response") == 0) {
        json_t *r = apisf_cron_error_response(in_str(&ctx, "exc_text", ""), &s);
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "expected_api_key") == 0) {
        char *r = apisf_expected_api_key(in_str(&ctx, "api_key", ""), &s);
        json_set(out, "ret", json_string(r ? r : ""));
        free(r);
    } else if (strcmp(func, "check_auth") == 0) {
        json_t *r = apisf_check_auth(request, in_str(&ctx, "api_key", ""), &s);
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "require_auth") == 0) {
        json_t *r = apisf_require_auth(request, &s);
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "cron_request_guard") == 0) {
        char *job_id = NULL;
        json_t *r = apisf_cron_request_guard(request, in_bool(&ctx, "need_job_id"),
                                             in_bool(&ctx, "check_draining"),
                                             json_get_bool(in, "cron_available", true), &s, &job_id);
        json_set(out, "ret", r ? r : json_null());
        json_set(out, "job_id", job_id ? json_string(job_id) : json_null());
        free(job_id);
    } else if (strcmp(func, "validate_cron_prompt") == 0) {
        json_t *r = apisf_validate_cron_prompt(in_str(&ctx, "prompt", ""),
                                               json_get_bool(in, "scan_available", true), &s);
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "parse_session_key_header") == 0) {
        char *key = NULL;
        json_t *r = apisf_parse_session_key_header(request, in_str(&ctx, "api_key", ""), &s, &key);
        json_set(out, "ret", r ? r : json_null());
        json_set(out, "key", key ? json_string(key) : json_null());
        free(key);
    } else if (strcmp(func, "sse_headers") == 0) {
        json_set(out, "ret", apisf_sse_headers(request, json_obj_get(in, "extra"), &s));
    } else if (strcmp(func, "approval_request_event") == 0) {
        json_t *r = apisf_approval_request_event(in_str(&ctx, "run_id", ""),
                                                 json_obj_get(in, "approval_data"),
                                                 json_obj_get(in, "fields"), &s);
        json_set(out, "ret", r);
    } else if (strcmp(func, "register_session_stream_approval") == 0) {
        json_t *state = json_copy(in_obj(&ctx, "state"));
        if (!state) state = json_object();
        apisf_register_session_stream_approval(state, in_str(&ctx, "run_id", ""));
        json_set(out, "ret", json_null());
        json_set(out, "state", state);
    } else if (strcmp(func, "session_stream_approval_notify") == 0) {
        apisf_session_stream_approval_notify(in_str(&ctx, "run_id", ""),
                                             json_obj_get(in, "approval_data"),
                                             in_str(&ctx, "message_id", ""), &s);
        json_set(out, "ret", json_null());
    } else if (strcmp(func, "queue_payload") == 0 || strcmp(func, "queue_enqueue") == 0 ||
               strcmp(func, "queue_settle_one") == 0 || strcmp(func, "queue_flush") == 0) {
        json_t *qstate = json_copy(in_obj(&ctx, "qstate"));
        if (!qstate) qstate = json_object();
        /* Both sides always carry the queue double array in the state. */
        if (!json_has(qstate, "queue")) json_set(qstate, "queue", json_array());
        ctx.qstate = qstate;
        json_t *payload = json_copy(json_obj_get(in, "payload"));
        if (!payload) payload = json_object();
        const char *name = in_str(&ctx, "name", "");
        if (strcmp(func, "queue_payload") == 0) {
            json_t *r = apisf_queue_payload(qstate, name, payload, &s);
            json_set(out, "ret", r);
        } else if (strcmp(func, "queue_enqueue") == 0) {
            apisf_queue_enqueue(qstate, name, payload, &s);
            json_set(out, "ret", json_null());
        } else if (strcmp(func, "queue_settle_one") == 0) {
            apisf_queue_settle_one(qstate);
            json_set(out, "ret", json_null());
        } else {
            apisf_queue_flush(qstate, &s);
            json_set(out, "ret", json_null());
        }
        json_set(out, "state", qstate);
        json_free(payload);
    } else if (strcmp(func, "idem_purge") == 0) {
        json_t *store = json_copy(in_obj(&ctx, "store"));
        if (!store) store = json_object();
        apisf_idem_purge(store, json_get_num(in, "ttl", 300),
                         (long)json_get_num(in, "max_items", 1000), &s);
        json_set(out, "ret", json_null());
        json_set(out, "state", store);
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    json_set(out, "trace", trace.events ? trace.events : json_array());
}

int main(int argc, char **argv) {
    FILE *f = (argc > 1) ? fopen(argv[1], "rb") : stdin;
    if (!f) { fprintf(stderr, "cannot open fixture\n"); return 1; }
    char line[1 << 20];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '#') continue;
        char *err = NULL;
        json_t *c = json_parse(line, &err);
        if (err || !c || c->type != JSON_OBJECT) {
            fprintf(stderr, "bad case line: %s\n", err ? err : "not an object");
            free(err);
            json_free(c);
            continue;
        }
        const char *func = json_get_str(c, "func", "");
        const json_t *in = json_obj_get(c, "in");
        json_t *empty = json_object();
        json_t *out = json_object();
        run_case(func, in ? in : empty, out);
        json_free(empty);
        char *in_s = json_serialize(in);
        char *out_s = json_serialize(out);
        char *func_s = json_escape_string(func);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", func_s,
               in_s ? in_s : "null", out_s ? out_s : "null");
        free(in_s); free(out_s); free(func_s);
        json_free(out);
        json_free(c);
    }
    if (f != stdin) fclose(f);
    return 0;
}
