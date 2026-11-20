/*
 * t_port_tui_hosted_room_service_flow.c — oracle harness for the FLOW port of
 * tui_gateway/hosted_room_service.py in src/gateway/tui_hosted_room_service_flow.c
 * (20 UNCLAIMED defs, all static thrs_*).
 *
 * Self-contained: includes the port .c by area-qualified name. Reads JSON-line
 * cases from stdin (tests/fixtures/tui_hosted_room_service_flow.jsonl), wires
 * the universal seam to the SCRIPTED results in each case's "seams" object (a
 * recording stub appending one trace event per seam call, in call order), and
 * emits {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 *
 * FAIL-CLOSED: a seam key MISSING from the script delivers
 * {"__raise__":"RuntimeError","msg":"unscripted seam: <name>"}; a present JSON
 * null is a valid None result; a JSON array script is one result PER CALL
 * (last repeats, empty array -> None). tests/oracle_tui_hosted_room_service_flow.py
 * drives the LIVE Python with the same scripts and compares the ordered
 * decision sequence + return value (raise envelopes included).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "gateway/tui_hosted_room_service_flow.c"

typedef struct {
    const json_t *in;
    thrs_trace_t *trace;
    json_t *counters;
} thrs_script_ctx_t;

static json_t *thrs_args_drop_null(const json_t *args) {
    json_t *out = json_object();
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++) {
            const json_t *v = args->c.items[i];
            if (v && v->type == JSON_NULL) continue;
            json_set(out, args->c.keys[i], json_copy(v));
        }
    }
    return out;
}

static json_t *thrs_script_call(void *vctx, const char *name, const json_t *args) {
    thrs_script_ctx_t *ctx = vctx;
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (!sc) {
        char *msg = thrs_xsf("unscripted seam: %s", name);
        result = thrs_raise_spec("RuntimeError", msg);
        free(msg);
    } else if (sc->type == JSON_ARRAY) {
        double idx = json_get_num(ctx->counters, name, 0);
        size_t i = (size_t)idx;
        size_t n = json_len(sc);
        if (n > 0) result = json_copy(json_get(sc, i < n ? i : n - 1));
        json_set(ctx->counters, name, json_number(idx + 1));
    } else {
        result = json_copy(sc);
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", thrs_args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    thrs_trace_add(ctx->trace, e);
    return result;
}

#define J(k) json_obj_get(in, k)

/* Normalize a raise envelope to {__raise__, msg[, code]} so extra scripted
 * keys never leak into the ret comparison. Non-raise rets pass through. */
static json_t *thrs_norm_raise(json_t *r) {
    if (!thrs_is_raise(r)) return r;
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(thrs_raise_class(r)));
    json_set(o, "msg", json_string(json_get_str(r, "msg", "")));
    const json_t *code = json_obj_get(r, "code");
    if (code) json_set(o, "code", json_copy(code));
    json_free(r);
    return o;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    thrs_trace_t trace = {NULL};
    json_t *counters = json_object();
    thrs_script_ctx_t ctx = {in, &trace, counters};
    thrs_seams_t seams = {&ctx, thrs_script_call};
    const thrs_seams_t *sp = &seams;
    json_t *ret = NULL;
    json_t *exc = NULL;
    json_t *empty = json_object();
    const json_t *self = J("self");
    if (!self) self = empty;

    if (!strcmp(func, "task_member_id")) {
        ret = thrs_task_member_id(J("task"), &exc);
    } else if (!strcmp(func, "approvable_attempts")) {
        ret = thrs_approvable_attempts(J("tasks"), &exc);
    } else if (!strcmp(func, "attempt_action")) {
        const char *kind = thrs_jstr(J("kind"), "retry");
        ret = thrs_attempt_action(kind, J("task"), &exc);
    } else if (!strcmp(func, "member_of")) {
        ret = thrs_member_of(J("task"));
    } else if (!strcmp(func, "authority")) {
        ret = thrs_authority(J("room"), &exc);
    } else if (!strcmp(func, "grant_revoke_is_terminal")) {
        ret = thrs_grant_revoke_is_terminal(J("exc"));
    } else if (!strcmp(func, "hosted_room_turn_timeout_seconds")) {
        ret = thrs_hosted_room_turn_timeout_seconds(sp);
    } else if (!strcmp(func, "hook")) {
        ret = thrs_hook(J("obj"), thrs_jstr(J("name"), ""));
    } else if (!strcmp(func, "disband_fence")) {
        ret = thrs_disband_fence(sp, J("service"), J("room_id"));
    } else if (!strcmp(func, "route_publication_lock")) {
        ret = thrs_route_publication_lock(J("service"));
    } else if (!strcmp(func, "member_activity")) {
        ret = thrs_member_activity(sp, J("task"), &exc);
    } else if (!strcmp(func, "attempt_actions")) {
        ret = thrs_attempt_actions(J("tasks_shown"), &exc);
    } else if (!strcmp(func, "stop_pending_refusal")) {
        ret = thrs_stop_pending_refusal();
    } else if (!strcmp(func, "policy_snapshot")) {
        ret = thrs_policy_snapshot(sp, J("room"), &exc);
    } else if (!strcmp(func, "profile_homes")) {
        ret = thrs_profile_homes(sp, self, &exc);
    } else if (!strcmp(func, "route_statuses")) {
        ret = thrs_route_statuses(self, J("room_id"));
    } else if (!strcmp(func, "events")) {
        ret = thrs_events(sp, J("room_id"), &exc);
    } else if (!strcmp(func, "append_room_status")) {
        ret = thrs_append_room_status(sp, J("room"), J("decision"), &exc);
    } else if (!strcmp(func, "publish_terminal")) {
        ret = thrs_publish_terminal(sp, J("binding"), J("task"), &exc);
    } else if (!strcmp(func, "peer_admission_receipt")) {
        ret = thrs_peer_admission_receipt(sp, self, J("room_id"), J("task"), &exc);
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    if (exc) {
        json_free(ret);
        ret = exc;
    }
    json_set(out, "ret", thrs_norm_raise(ret ? ret : json_null()));
    json_set(out, "trace", trace.events ? json_copy(trace.events) : json_array());
    thrs_trace_free(&trace);
    json_free(counters);
    json_free(empty);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    while ((len = getline(&line, &cap, stdin)) > 0) {
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        if (line[0] != '{') continue;
        char *err = NULL;
        json_t *c = json_parse(line, &err);
        if (err || !c || c->type != JSON_OBJECT) {
            fprintf(stderr, "bad case: %s\n", err ? err : "not object");
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
    free(line);
    return 0;
}
