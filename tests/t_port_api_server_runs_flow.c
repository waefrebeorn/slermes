/*
 * t_port_api_server_runs_flow.c — oracle harness for the run-lifecycle FLOW
 * cores ported in src/gateway/platforms/api_server_runs_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * Wires each flow's seams to the SCRIPTED results in "in" (recording stubs
 * that append one trace event per seam call, in call order) and emits
 * {"func": ..., "in": ..., "out": {"ret": ..., "trace": [...], ...}} per case.
 * tests/oracle_api_server_runs_flow.py drives the LIVE Python with the same
 * scripts. The port .c is #included directly (self-contained; the gate links
 * only lib/libjson/json.o).
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
#include "gateway/platforms/api_server_runs_flow.c"

typedef struct {
    const json_t *in;
    asrun_trace_t *trace;
    json_t *req_state;   /* request double for request_set */
    size_t clock_idx;
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
static void rec(script_ctx_t *c, json_t *e) { asrun_trace_add(c->trace, e); }

static const json_t *in_obj(const script_ctx_t *c, const char *k) {
    const json_t *v = json_obj_get(c->in, k);
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}
static bool in_bool(const script_ctx_t *c, const char *k) {
    return json_get_bool(c->in, k, false);
}
static const char *in_str(const script_ctx_t *c, const char *k, const char *def) {
    return json_get_str(c->in, k, def);
}
/* Scripted string or NULL (a JSON null / missing key reads as None). */
static const char *in_str_or_null(const script_ctx_t *c, const char *k) {
    const json_t *v = json_obj_get(c->in, k);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
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
static json_t *sf_openai_error(void *ctx, const char *message, const json_t *kwargs) {
    script_ctx_t *c = ctx;
    json_t *e = ev("openai_error");
    evs(e, "message", message);
    evj(e, "kwargs", kwargs);
    rec(c, e);
    json_t *r = json_object();
    json_set(r, "message", json_string(message ? message : ""));
    asrun_obj_update(r, kwargs);
    return r;
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
static char *sf_redact(void *ctx, const char *text) {
    script_ctx_t *c = ctx;
    const char *r = json_get_str(c->in, "redact_ret", text ? text : "");
    json_t *e = ev("redact");
    evs(e, "text", text);
    evs(e, "ret", r);
    rec(c, e);
    return strdup(r ? r : "");
}
static const char *sf_room_grant_token(void *ctx, const json_t *request) {
    script_ctx_t *c = ctx;
    (void)request;
    const char *r = in_str_or_null(c, "room_token");
    json_t *e = ev("room_grant_token");
    evs(e, "ret", r);
    rec(c, e);
    return r;
}
static json_t *sf_room_run_claims(void *ctx, const json_t *request, const char *permission,
                                  bool *raised, const char **exc_type, const char **exc_msg) {
    script_ctx_t *c = ctx;
    (void)request;
    *raised = in_bool(c, "claims_raises");
    *exc_type = in_str(c, "claims_exc_type", "RuntimeError");
    *exc_msg = in_str(c, "claims_exc_msg", "grant invalid");
    const json_t *r = in_obj(c, "claims_ret");
    json_t *e = ev("room_run_claims");
    evs(e, "permission", permission);
    if (*raised) {
        evs(e, "raised", *exc_type);
        evs(e, "msg", *exc_msg);
    } else {
        evj(e, "ret", r);
    }
    rec(c, e);
    return (*raised || !r) ? NULL : json_copy(r);
}
static const char *sf_expected_api_key(void *ctx) {
    script_ctx_t *c = ctx;
    const char *r = in_str_or_null(c, "api_key_ret");
    json_t *e = ev("expected_api_key");
    evs(e, "ret", r);
    rec(c, e);
    return r;
}
static const char *sf_request_profile(void *ctx) {
    script_ctx_t *c = ctx;
    const char *r = in_str_or_null(c, "profile");
    json_t *e = ev("request_profile");
    evs(e, "ret", r);
    rec(c, e);
    return r;
}
static bool sf_request_set(void *ctx, json_t *req_state, const char *key, double value,
                           const char **exc_type) {
    script_ctx_t *c = ctx;
    const char *t = in_str_or_null(c, "request_set_raises");
    json_t *e = ev("request_set");
    evs(e, "key", key);
    evn(e, "value", value);
    if (t) evs(e, "raised", t);
    rec(c, e);
    if (t) { *exc_type = t; return false; }
    json_t *items = json_obj_get(req_state, "items");
    if (items && items->type == JSON_OBJECT) json_set(items, key, json_number(value));
    return true;
}
static double sf_room_retention_until(void *ctx, const json_t *request) {
    script_ctx_t *c = ctx;
    (void)request;
    double r = json_get_num(c->in, "retention_until", 0.0);
    json_t *e = ev("room_retention_until");
    evn(e, "ret", r);
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
    /* Seam contract: NULL == None. A scripted JSON null must come back as a
     * NULL pointer, not a json_null node the flow would read as an error. */
    return (r && r->type != JSON_NULL) ? json_copy(r) : NULL;
}
static bool sf_pid_exists(void *ctx, long pid, bool *raised) {
    script_ctx_t *c = ctx;
    *raised = in_bool(c, "pid_exists_raises");
    bool r = in_bool(c, "pid_exists_ret");
    json_t *e = ev("pid_exists");
    evn(e, "pid", (double)pid);
    if (*raised) evb(e, "raised", true);
    else evb(e, "ret", r);
    rec(c, e);
    return r;
}
static double sf_process_start_time(void *ctx, long pid, bool *raised, bool *is_none) {
    script_ctx_t *c = ctx;
    *raised = in_bool(c, "start_time_raises");
    *is_none = false;
    const json_t *r = json_obj_get(c->in, "start_time_ret");
    double v = 0.0;
    json_t *e = ev("process_start_time");
    evn(e, "pid", (double)pid);
    if (*raised) {
        evb(e, "raised", true);
    } else if (!r || r->type == JSON_NULL) {
        *is_none = true;
        evs(e, "ret", NULL);
    } else {
        v = (r->type == JSON_NUMBER) ? r->num_val : 0.0;
        evn(e, "ret", v);
    }
    rec(c, e);
    return v;
}
static bool sf_fingerprints_match(void *ctx, long recorded, double current, bool *raised) {
    script_ctx_t *c = ctx;
    *raised = in_bool(c, "fp_match_raises");
    bool r = in_bool(c, "fp_match_ret");
    json_t *e = ev("fingerprints_match");
    evn(e, "recorded", (double)recorded);
    evn(e, "current", current);
    if (*raised) evb(e, "raised", true);
    else evb(e, "ret", r);
    rec(c, e);
    return r;
}
static json_t *sf_run_projection(void *ctx, const char *run_id) {
    script_ctx_t *c = ctx;
    const json_t *r = json_obj_get(c->in, "projection_ret");
    json_t *e = ev("run_projection");
    evs(e, "run_id", run_id);
    evj(e, "ret", r);
    rec(c, e);
    return (r && r->type != JSON_NULL) ? json_copy(r) : NULL;
}
static void sf_store_extend_retention(void *ctx, const char *scope, const char *run_id,
                                      double retention_until) {
    script_ctx_t *c = ctx;
    json_t *e = ev("store_extend_retention");
    evs(e, "scope", scope);
    evs(e, "run_id", run_id);
    evn(e, "retention_until", retention_until);
    rec(c, e);
}
static json_t *sf_store_status_for_run(void *ctx, const char *scope, const char *run_id,
                                       double retention_until) {
    script_ctx_t *c = ctx;
    const json_t *r = json_obj_get(c->in, "store_record");
    json_t *e = ev("store_status_for_run");
    evs(e, "scope", scope);
    evs(e, "run_id", run_id);
    evn(e, "retention_until", retention_until);
    evj(e, "ret", r);
    rec(c, e);
    return (r && r->type != JSON_NULL) ? json_copy(r) : NULL;
}
static void sf_store_update_status(void *ctx, const char *run_id, const json_t *status,
                                   bool *raised, const char **exc_type, const char **exc_msg) {
    script_ctx_t *c = ctx;
    *raised = in_bool(c, "store_update_raises");
    *exc_type = in_str(c, "store_update_exc_type", "RuntimeError");
    *exc_msg = in_str(c, "store_update_exc_msg", "store failed");
    json_t *e = ev("store_update_status");
    evs(e, "run_id", run_id);
    evj(e, "status", status);
    if (*raised) {
        evs(e, "raised", *exc_type);
        evs(e, "msg", *exc_msg);
    }
    rec(c, e);
}
static json_t *sf_response_store_get(void *ctx, const char *response_id) {
    script_ctx_t *c = ctx;
    const json_t *r = json_obj_get(c->in, "stored_ret");
    json_t *e = ev("response_store_get");
    evs(e, "id", response_id);
    evj(e, "ret", r);
    rec(c, e);
    return (r && r->type != JSON_NULL) ? json_copy(r) : NULL;
}

static asrun_seams_t wired_seams(script_ctx_t *c) {
    asrun_seams_t s = {
        .ctx = c,
        .json_response = sf_json_response,
        .openai_error = sf_openai_error,
        .log = sf_log,
        .clock = sf_clock,
        .redact = sf_redact,
        .room_grant_token = sf_room_grant_token,
        .room_run_claims = sf_room_run_claims,
        .expected_api_key = sf_expected_api_key,
        .request_profile = sf_request_profile,
        .request_set = sf_request_set,
        .room_retention_until = sf_room_retention_until,
        .check_auth = sf_check_auth,
        .pid_exists = sf_pid_exists,
        .process_start_time = sf_process_start_time,
        .fingerprints_match = sf_fingerprints_match,
        .run_projection = sf_run_projection,
        .store_extend_retention = sf_store_extend_retention,
        .store_status_for_run = sf_store_status_for_run,
        .store_update_status = sf_store_update_status,
        .response_store_get = sf_response_store_get,
    };
    return s;
}

/* ── dispatch ── */
static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = { in, NULL, NULL, 0 };
    asrun_trace_t trace = { NULL };
    ctx.trace = &trace;
    asrun_seams_t s = wired_seams(&ctx);
    const json_t *request = json_obj_get(in, "request");
    const char *run_id = in_str(&ctx, "run_id", "");
    const char *gsk = in_str_or_null(&ctx, "gateway_session_key");
    char *err = NULL;
    json_t *state = NULL;
    json_t *req = NULL;
    bool emit_state = false, emit_req = false;

    if (strcmp(func, "terminal_run_status") == 0) {
        char *status = NULL;
        json_t *fields = asrun_terminal_run_status(json_obj_get(in, "result"), &status);
        json_t *ret = json_array();
        json_append(ret, json_string(status ? status : ""));
        json_append(ret, fields);
        free(status);
        json_set(out, "ret", ret);
    } else if (strcmp(func, "run_user_message") == 0) {
        json_t *err_desc = NULL;
        json_t *msg = asrun_run_user_message(json_obj_get(in, "raw_input"), &s, &err_desc, &err);
        json_t *ret = json_array();
        json_append(ret, msg ? msg : json_null());
        json_append(ret, err_desc ? err_desc : json_null());
        json_set(out, "ret", ret);
    } else if (strcmp(func, "call_id") == 0) {
        json_set(out, "ret", asrun_call_id(json_obj_get(in, "kw")));
    } else if (strcmp(func, "tool_completed_preview") == 0) {
        char *r = asrun_tool_completed_preview(json_obj_get(in, "result"), &s);
        json_set(out, "ret", json_string(r ? r : ""));
        free(r);
    } else if (strcmp(func, "run_event") == 0) {
        json_t *r = asrun_run_event(run_id, in_str(&ctx, "name", ""),
                                    json_obj_get(in, "fields"), &s, &err);
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "room_permission_for") == 0) {
        json_set(out, "ret", json_string(asrun_room_permission_for(request)));
    } else if (strcmp(func, "room_run_scope") == 0) {
        char *r = asrun_room_scope(json_obj_get(in, "claims"), &err);
        json_set(out, "ret", r ? json_string(r) : json_null());
        free(r);
    } else if (strcmp(func, "remember_room_retention") == 0) {
        req = asrun_req_state_new(in);
        ctx.req_state = req;
        asrun_remember_room_retention(req, json_obj_get(in, "claims"), &s, &err);
        json_set(out, "ret", json_null());
        emit_req = true;
    } else if (strcmp(func, "run_idempotency_scope") == 0) {
        req = asrun_req_state_new(in);
        ctx.req_state = req;
        char *r = asrun_run_idempotency_scope(request, req, &s, &err);
        json_set(out, "ret", r ? json_string(r) : json_null());
        free(r);
        emit_req = true;
    } else if (strcmp(func, "check_run_auth") == 0) {
        json_t *r = asrun_check_run_auth(request, in_str(&ctx, "permission", "status"), &s);
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "owner_alive") == 0) {
        bool r = asrun_owner_alive((long)json_get_num(in, "owner_pid", 0),
                                   (long)json_get_num(in, "owner_started", 0), &s);
        json_set(out, "ret", json_bool(r));
    } else if (strcmp(func, "set_run_status") == 0) {
        state = asrun_state_new(json_obj_get(in, "state"));
        json_t *r = asrun_set_run_status(state, run_id, in_str(&ctx, "status", "running"),
                                         json_obj_get(in, "fields"), &s);
        json_set(out, "ret", r);
        emit_state = true;
    } else if (strcmp(func, "mark_shutdown_requested") == 0) {
        state = asrun_state_new(json_obj_get(in, "state"));
        long n = asrun_mark_shutdown_requested(state, &s);
        json_set(out, "ret", json_number((double)n));
        emit_state = true;
    } else if (strcmp(func, "forget_run") == 0) {
        state = asrun_state_new(json_obj_get(in, "state"));
        asrun_forget_run(state, run_id, json_obj_get(in, "tables"));
        json_set(out, "ret", json_null());
        emit_state = true;
    } else if (strcmp(func, "release_run_owner_if_forgotten") == 0) {
        state = asrun_state_new(json_obj_get(in, "state"));
        asrun_release_run_owner_if_forgotten(state, run_id);
        json_set(out, "ret", json_null());
        emit_state = true;
    } else if (strcmp(func, "durable_run_status") == 0) {
        state = asrun_state_new(json_obj_get(in, "state"));
        req = asrun_req_state_new(in);
        ctx.req_state = req;
        json_t *r = asrun_durable_run_status(state, request, req, run_id, &s, &err);
        json_set(out, "ret", r ? r : json_null());
        emit_state = true;
        emit_req = true;
    } else if (strcmp(func, "resolve_conversation_history") == 0) {
        json_t *instructions = NULL, *stored = NULL, *err_desc = NULL;
        json_t *hist = asrun_resolve_conversation_history(
            json_obj_get(in, "body"), json_obj_get(in, "raw_input"), &s,
            &instructions, &stored, &err_desc);
        json_t *ret = json_array();
        json_append(ret, hist);
        json_append(ret, instructions ? instructions : json_null());
        json_append(ret, stored ? stored : json_null());
        json_append(ret, err_desc ? err_desc : json_null());
        json_set(out, "ret", ret);
    } else if (strcmp(func, "accepted_response") == 0) {
        const json_t *st = json_obj_get(in, "status");
        json_t *stn = st ? json_copy(st) : json_string("queued");
        json_t *r = asrun_accepted_response(run_id, stn, gsk, in_bool(&ctx, "replayed"), &s);
        json_free(stn);
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "replay_or_conflict") == 0) {
        state = asrun_state_new(json_obj_get(in, "state"));
        req = asrun_req_state_new(in);
        ctx.req_state = req;
        json_t *r = asrun_replay_or_conflict(state, request, req,
                                             in_str(&ctx, "outcome", "reused"),
                                             json_obj_get(in, "record"), gsk, &s, &err);
        json_set(out, "ret", r ? r : json_null());
        emit_state = true;
        emit_req = true;
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    /* Error convention matches the oracle's catch-all: {"ret": null, "err":
     * "<ExcType>: <msg>"} with NO state snapshot (the live drive discards the
     * mutated double when the call raises). */
    if (err) {
        json_set(out, "ret", json_null());
        json_set(out, "err", json_string(err));
        free(err);
        json_free(state);
        json_free(req);
    } else {
        if (emit_state && state) json_set(out, "state", asrun_state_out(state));
        if (emit_req && req) json_set(out, "request_state", req);
        json_free(state);
        if (!emit_req) json_free(req);
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