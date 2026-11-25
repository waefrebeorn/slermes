/*
 * t_port_web_server_flow2.c — oracle harness for the REMAINING UNCLAIMED
 * web-server FLOW surface of hermes_cli/web_server.py ported in
 * src/cli/web_server_flow2.c (uvicorn build / post-bind arming / server-runtime
 * orchestrators + their nested closures and lambdas).
 *
 * Self-contained: includes the port .c by area-qualified name. Reads JSON-line
 * cases from stdin (tests/fixtures/web_server_flow2.jsonl), wires the universal
 * seam to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...][, "state":...]}}.
 *
 * FAIL-CLOSED: a seam key MISSING from the script delivers
 * {"__raise__":"RuntimeError","msg":"unscripted seam: <name>"}; a present JSON
 * null is a valid None result; a JSON array script is one result PER CALL
 * (last repeats, empty array -> None). tests/oracle_web_server_flow2.py drives
 * the LIVE Python with the same scripts and compares the ordered decision
 * sequence + return value (raise envelopes included) + the mutated state window.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/web_server_flow2.c"

typedef struct {
    const json_t *in;
    ws2_trace_t *trace;
    json_t *counters;
} ws2_script_ctx_t;

static json_t *ws2_args_drop_null(const json_t *args) {
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

static json_t *ws2_script_call(void *vctx, const char *name, const json_t *args) {
    ws2_script_ctx_t *ctx = vctx;
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (!sc) {
        char *msg = ws2_xsf("unscripted seam: %s", name);
        result = ws2_raise_spec("RuntimeError", msg);
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
    json_set(e, "args", ws2_args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    ws2_trace_add(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define J(k) json_obj_get(in, k)
#define N(k, d) json_get_num(in, k, d)
#define B(k) ws2_jtruth(json_obj_get(in, k))

/* Normalize a raise envelope to {__raise__, msg[, code]} so extra scripted
 * keys never leak into the ret comparison. Non-raise rets pass through. */
static json_t *ws2_norm_raise(json_t *r) {
    if (!ws2_is_raise(r)) return r;
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(ws2_raise_class(r)));
    json_set(o, "msg", json_string(json_get_str(r, "msg", "")));
    const json_t *code = json_obj_get(r, "code");
    if (code) json_set(o, "code", json_copy(code));
    json_free(r);
    return o;
}

static json_t *copy_obj_or_empty(const json_t *v) {
    return (v && v->type == JSON_OBJECT) ? json_copy(v) : json_object();
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    ws2_trace_t trace = {NULL};
    json_t *counters = json_object();
    ws2_script_ctx_t ctx = {in, &trace, counters};
    ws2_seams_t seams = {&ctx, ws2_script_call};
    const ws2_seams_t *sp = &seams;
    json_t *ret = NULL;
    json_t *workstate = NULL;   /* app.state working copy */
    json_t *workserver = NULL;  /* server working copy (serve) */
    json_t *state_echo = NULL;
    bool echo_state = false;

    if (!strcmp(func, "build_uvicorn_server")) {
        workstate = copy_obj_or_empty(J("app_state"));
        ret = ws2_build_uvicorn_server(sp, workstate, S("host") ? S("host") : "127.0.0.1",
                                       N("port", 9119), B("ssh_isolated"));
        echo_state = true;
    } else if (!strcmp(func, "ws_ping_setting")) {
        const json_t *dc = J("dash_cfg");
        json_t *empty = json_object();
        json_t *defval = json_number(N("default", 20.0));
        ret = ws2_ws_ping_setting(sp, dc ? dc : empty, S("key") ? S("key") : "ws_ping_interval", defval);
        json_free(defval);
        json_free(empty);
    } else if (!strcmp(func, "on_server_started")) {
        workstate = copy_obj_or_empty(J("app_state"));
        const json_t *srv = J("server");
        json_t *empty = json_object();
        ret = ws2_on_server_started(sp, workstate, srv ? srv : empty,
                                    S("host") ? S("host") : "127.0.0.1", N("port", 9119),
                                    B("headless"), B("isolated"), B("open_browser"),
                                    S("initial_profile") ? S("initial_profile") : "",
                                    B("start_mcp_discovery_after_bind"),
                                    J("ssh_lock_path"), J("ssh_owner_nonce"));
        json_free(empty);
        echo_state = true;
    } else if (!strcmp(func, "reap_desktop_serves")) {
        ret = ws2_reap_desktop_serves(sp);
    } else if (!strcmp(func, "reap_mcp_helpers")) {
        ret = ws2_reap_mcp_helpers(sp);
    } else if (!strcmp(func, "register_identity")) {
        ret = ws2_register_identity(sp, B("headless"), S("host") ? S("host") : "",
                                    J("actual_port"), S("initial_profile") ? S("initial_profile") : "",
                                    B("isolated"));
    } else if (!strcmp(func, "install_noise_filter")) {
        ret = ws2_install_noise_filter(sp);
    } else if (!strcmp(func, "loop_heartbeat")) {
        const json_t *loop = J("loop");
        json_t *empty = json_object();
        json_t *interval = json_number(N("interval", 2.0));
        json_t *threshold = json_number(N("threshold", 5.0));
        ret = ws2_loop_heartbeat(sp, loop ? loop : empty, J("expected"), interval, threshold);
        json_free(interval);
        json_free(threshold);
        json_free(empty);
    } else if (!strcmp(func, "publish_rendezvous_step")) {
        ret = ws2_publish_rendezvous_step(sp, S("host") ? S("host") : "", J("actual_port"));
    } else if (!strcmp(func, "start_server_runtime")) {
        workstate = copy_obj_or_empty(J("app_state"));
        ret = ws2_start_server_runtime(sp, workstate, S("host") ? S("host") : "127.0.0.1",
                                       N("port", 9119), B("open_browser"), B("allow_public"),
                                       S("initial_profile") ? S("initial_profile") : "",
                                       B("headless"), B("isolated"),
                                       J("ssh_session_token"), J("ssh_owner_nonce"),
                                       B("start_mcp_discovery_after_bind"), J("ssh_lock_path"));
        echo_state = true;
    } else if (!strcmp(func, "serve")) {
        workstate = copy_obj_or_empty(J("app_state"));
        workserver = copy_obj_or_empty(J("server"));
        const json_t *cfg = J("config");
        json_t *empty = json_object();
        ret = ws2_serve(sp, cfg ? cfg : empty, workserver, workstate,
                        S("host") ? S("host") : "127.0.0.1", N("port", 9119),
                        B("headless"), B("isolated"), B("open_browser"),
                        S("initial_profile") ? S("initial_profile") : "",
                        B("start_mcp_discovery_after_bind"),
                        J("ssh_lock_path"), J("ssh_owner_nonce"));
        json_free(empty);
        state_echo = json_object();
        json_set(state_echo, "server", json_copy(workserver));
        json_set(state_echo, "app_state", json_copy(workstate));
        echo_state = true;
    } else if (!strcmp(func, "cron_owned")) {
        ret = ws2_cron_owned(sp, S("own_name") ? S("own_name") : "default",
                             S("own_home") ? S("own_home") : "");
    } else if (!strcmp(func, "cron_profile_gate")) {
        ret = ws2_cron_profile_gate(sp, S("name") ? S("name") : "default",
                                    S("home") ? S("home") : "");
    } else if (!strcmp(func, "cron_profile_homes")) {
        ret = ws2_cron_profile_homes(sp);
    } else if (!strcmp(func, "cron_own_home_fallback")) {
        ret = ws2_cron_own_home_fallback(S("own_name") ? S("own_name") : "default",
                                         S("own_home") ? S("own_home") : "");
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    json_set(out, "ret", ws2_norm_raise(ret ? ret : json_null()));
    json_set(out, "trace", trace.events ? json_copy(trace.events) : json_array());
    if (echo_state) {
        json_t *st = state_echo ? state_echo : workstate;
        json_set(out, "state", st ? json_copy(st) : json_object());
    }
    ws2_trace_free(&trace);
    json_free(counters);
    if (workstate) json_free(workstate);
    if (workserver) json_free(workserver);
    if (state_echo) json_free(state_echo);
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