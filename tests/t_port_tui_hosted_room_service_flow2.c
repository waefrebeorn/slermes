/*
 * t_port_tui_hosted_room_service_flow2.c — oracle harness for the FLOW2 port
 * of tui_gateway/hosted_room_service.py in
 * src/gateway/tui_hosted_room_service_flow2.c (15 disjoint UNCLAIMED defs).
 *
 * Self-contained: includes the port .c by area-qualified name. Reads JSON-line
 * cases from stdin, wires the universal seam to the SCRIPTED results in each
 * case's "seams" object (a recording stub appending one trace event per seam
 * call, in call order) and emits
 *   {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 *
 * FAIL-CLOSED: a seam key MISSING from the script delivers
 * {"__raise__":"RuntimeError","msg":"unscripted seam: <name>"}; a present JSON
 * null is a valid None; a JSON array script is one result PER CALL (last
 * repeats, empty -> None). tests/oracle_tui_hosted_room_service_flow2.py drives
 * the LIVE Python with the same scripts and compares the ordered trace + return.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "gateway/tui_hosted_room_service_flow2.c"
/* MinGW UCRT may not expose getline under strict C11 even with _GNU_SOURCE;
 * this compatible declaration keeps the harness self-contained. */
extern ssize_t getline(char **, size_t *, FILE *);

typedef struct {
    const json_t *in;
    thrs2_trace_t *trace;
    json_t *counters;
} thrs2_script_ctx_t;

static json_t *thrs2_args_drop_null(const json_t *args) {
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

static json_t *thrs2_script_call(void *vctx, const char *name, const json_t *args) {
    thrs2_script_ctx_t *ctx = vctx;
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (!sc) {
        char *msg = thrs2_xsf("unscripted seam: %s", name);
        result = thrs2_raise_spec("RuntimeError", msg);
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
    json_set(e, "args", thrs2_args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    thrs2_trace_add(ctx->trace, e);
    return result;
}

#define J(k) json_obj_get(in, k)

static json_t *thrs2_norm_raise(json_t *r) {
    if (!thrs2_is_raise(r)) return r;
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(thrs2_raise_class(r)));
    json_set(o, "msg", json_string(json_get_str(r, "msg", "")));
    const json_t *code = json_obj_get(r, "code");
    if (code) json_set(o, "code", json_copy(code));
    json_free(r);
    return o;
}

static double thrs2_in_num(const json_t *in, const char *key, double def) {
    const json_t *v = json_obj_get(in, key);
    return thrs2_is_num(v) ? v->num_val : def;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    thrs2_trace_t trace = {NULL};
    json_t *counters = json_object();
    thrs2_script_ctx_t ctx = {in, &trace, counters};
    thrs2_seams_t seams = {&ctx, thrs2_script_call};
    const thrs2_seams_t *sp = &seams;
    json_t *ret = NULL;
    json_t *exc = NULL;
    json_t *empty = json_object();
    const json_t *self = J("self");
    if (!self) self = empty;

    if (!strcmp(func, "live_room_services")) {
        ret = thrs2_live_room_services(sp, &exc);
    } else if (!strcmp(func, "member_is_peer")) {
        ret = thrs2_member_is_peer(sp, J("room_id"), J("member_id"), &exc);
    } else if (!strcmp(func, "owned_authority")) {
        ret = thrs2_owned_authority(sp, J("room_id"), &exc);
    } else if (!strcmp(func, "route_authority")) {
        ret = thrs2_route_authority(sp, self, J("room_id"), &exc);
    } else if (!strcmp(func, "bindings")) {
        ret = thrs2_bindings(sp, &exc);
    } else if (!strcmp(func, "list_tasks")) {
        ret = thrs2_list_tasks(sp, J("room_id"), J("statuses"), &exc);
    } else if (!strcmp(func, "turn_lock")) {
        ret = thrs2_turn_lock(sp, json_get_str(self, "root", ""),
                              thrs2_jstr(J("profile"), ""), &exc);
    } else if (!strcmp(func, "start")) {
        ret = thrs2_start(sp, &exc);
    } else if (!strcmp(func, "stop")) {
        ret = thrs2_stop(sp, thrs2_in_num(in, "timeout", 5.0), &exc);
    } else if (!strcmp(func, "wakeup")) {
        ret = thrs2_wakeup(sp, &exc);
    } else if (!strcmp(func, "save_link")) {
        ret = thrs2_save_link(sp, J("link"), &exc);
    } else if (!strcmp(func, "dispatch_approval")) {
        ret = thrs2_dispatch_approval(sp, J("route"), J("client"), J("action"),
                thrs2_jstr(J("task_id"), ""), thrs2_in_num(in, "execution_generation", 0),
                thrs2_jstr(J("request_id"), ""), thrs2_jstr(J("choice"), ""), &exc);
    } else if (!strcmp(func, "room_profiles")) {
        ret = thrs2_room_profiles(sp, &exc);
    } else if (!strcmp(func, "make_rpc")) {
        ret = thrs2_make_rpc(sp, self, J("server"), &exc);
    } else if (!strcmp(func, "set_route_status")) {
        ret = thrs2_set_route_status(sp, self, J("room_id"), J("member_id"), J("status"), &exc);
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    if (exc) { json_free(ret); ret = exc; }
    json_set(out, "ret", thrs2_norm_raise(ret ? ret : json_null()));
    json_set(out, "trace", trace.events ? json_copy(trace.events) : json_array());
    thrs2_trace_free(&trace);
    json_free(counters);
    json_free(empty);
}

int main(void) {
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
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
