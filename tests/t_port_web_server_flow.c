/*
 * t_port_web_server_flow.c — oracle harness for the UNCLAIMED web-server core
 * FLOW surface of hermes_cli/web_server.py ported in src/cli/web_server_flow.c.
 *
 * Self-contained: includes the port .c by area-qualified name. Reads JSON-line
 * cases from stdin (tests/fixtures/web_server_flow.jsonl), wires the universal
 * seam to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...]}} (plus "state" for
 * the mutation flows: app_state_default, get_pty_active_session_files,
 * apply_ssh_session_token, apply_ssh_owner_nonce, record_selftest,
 * configure_auth_gate, publish_host_rendezvous).
 *
 * FAIL-CLOSED: a seam key MISSING from the script delivers
 * {"__raise__":"RuntimeError","msg":"unscripted seam: <name>"}; a present JSON
 * null is a valid None result; a JSON array script is one result PER CALL
 * (last repeats, empty array -> None). tests/oracle_web_server_flow.py drives
 * the LIVE Python with the same scripts and compares the ordered decision
 * sequence + return value (raise envelopes included).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/web_server_flow.c"

typedef struct {
    const json_t *in;
    ws_trace_t *trace;
    json_t *counters;
} ws_script_ctx_t;

static json_t *ws_args_drop_null(const json_t *args) {
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

static json_t *ws_script_call(void *vctx, const char *name, const json_t *args) {
    ws_script_ctx_t *ctx = vctx;
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (!sc) {
        /* fail-closed: an unscripted seam raises RuntimeError on both sides */
        char *msg = ws_xsf("unscripted seam: %s", name);
        result = ws_raise_spec("RuntimeError", msg);
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
    json_set(e, "args", ws_args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    ws_trace_add(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define J(k) json_obj_get(in, k)

/* Normalize a raise envelope to {__raise__, msg[, code]} so extra scripted
 * keys never leak into the ret comparison (the oracle envelope carries only
 * these; SystemExit additionally carries code). Non-raise rets pass through. */
static json_t *ws_norm_raise(json_t *r) {
    if (!ws_is_raise(r)) return r;
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(ws_raise_class(r)));
    /* LIVE raises SystemExit(code): str(SystemExit(c)) is str(c), the decimal
     * string of a numeric code, NOT the scripted "msg". Mirror that so a
     * scripted msg that disagrees with code does not leak into the envelope. */
    const json_t *code = json_obj_get(r, "code");
    if (code && !strcmp(ws_raise_class(r), "SystemExit")) {
        char num[64];
        const char *cs = NULL;
        if (code->type == JSON_NUMBER) {
            if (code->num_val == (double)(long long)code->num_val)
                snprintf(num, sizeof num, "%lld", (long long)code->num_val);
            else
                snprintf(num, sizeof num, "%.17g", code->num_val);
            cs = num;
        } else if (code->type == JSON_STRING) {
            cs = code->str_val ? code->str_val : "";
        }
        json_set(o, "msg", json_string(cs ? cs : json_get_str(r, "msg", "")));
    } else {
        json_set(o, "msg", json_string(json_get_str(r, "msg", "")));
    }
    if (code) json_set(o, "code", json_copy(code));
    json_free(r);
    return o;
}

/* canonical request dict (defaults filled identically in the oracle) */
static json_t *ws_h_request(const json_t *in) {
    const json_t *r = J("request");
    json_t *out = json_object();
    const json_t *st = r ? json_obj_get(r, "state") : NULL;
    json_set(out, "state", st ? json_copy(st) : json_object());
    const json_t *app = r ? json_obj_get(r, "app") : NULL;
    const json_t *appst = app ? json_obj_get(app, "state") : NULL;
    json_t *appo = json_object();
    json_set(appo, "state", appst ? json_copy(appst) : json_object());
    json_set(out, "app", appo);
    const json_t *h = r ? json_obj_get(r, "headers") : NULL;
    json_set(out, "headers", h ? json_copy(h) : json_object());
    const json_t *q = r ? json_obj_get(r, "query_params") : NULL;
    json_set(out, "query_params", q ? json_copy(q) : json_object());
    const json_t *url = r ? json_obj_get(r, "url") : NULL;
    json_t *urlo = json_object();
    json_set(urlo, "path", json_string(json_get_str(url, "path", "")));
    json_set(out, "url", urlo);
    const json_t *sc = r ? json_obj_get(r, "scope") : NULL;
    json_set(out, "scope", sc ? json_copy(sc) : json_object());
    return out;
}

/* DashboardHealth default state (__init__ defaults) + fixture overlay */
static json_t *ws_h_health(const json_t *in) {
    json_t *h = json_object();
    json_set(h, "window_seconds", json_number(300.0));
    json_set(h, "_error_times", json_array());
    json_set(h, "last_error_type", json_null());
    json_set(h, "last_error_path", json_null());
    json_set(h, "last_error_at", json_null());
    json_set(h, "selftest_status", json_string("unknown"));
    json_set(h, "selftest_http_status", json_null());
    json_set(h, "selftest_at", json_null());
    const json_t *ov = J("health");
    if (ov && ov->type == JSON_OBJECT) {
        for (size_t i = 0; i < ov->c.count; i++)
            json_set(h, ov->c.keys[i], json_copy(ov->c.items[i]));
    }
    return h;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    ws_trace_t trace = {NULL};
    json_t *counters = json_object();
    ws_script_ctx_t ctx = {in, &trace, counters};
    ws_seams_t seams = {&ctx, ws_script_call};
    const ws_seams_t *sp = &seams;
    json_t *ret = NULL;
    json_t *req = NULL;        /* canonical request owned by the harness */
    json_t *workstate = NULL;  /* mutation-flow working copy */
    json_t *echo = NULL;       /* fresh echo object for ssh globals */
    bool echo_state = false;

    if (!strcmp(func, "gateway_owns_cron")) {
        ret = ws_gateway_owns_cron(sp, json_get_str(in, "name", "default"),
                                   json_get_str(in, "home", ""));
    } else if (!strcmp(func, "app_state_default")) {
        workstate = J("state") ? json_copy(J("state")) : json_object();
        const json_t *fk = J("factory");
        const char *kind = fk ? json_get_str(fk, "__callable__", "factory") : "factory";
        ret = ws_app_state_default(sp, workstate, json_get_str(in, "name", ""), kind);
        echo_state = true;
    } else if (!strcmp(func, "get_pty_active_session_files")) {
        workstate = J("state") ? json_copy(J("state")) : json_object();
        ret = ws_get_pty_active_session_files(sp, workstate);
        echo_state = true;
    } else if (!strcmp(func, "apply_ssh_session_token")) {
        workstate = J("state") ? json_copy(J("state")) : json_object();
        ws_apply_ssh_session_token(workstate, J("token"));
        echo = json_object();
        const json_t *tok = json_obj_get(workstate, "_SESSION_TOKEN");
        json_set(echo, "_SESSION_TOKEN", tok ? json_copy(tok) : json_string(""));
        echo_state = true;
    } else if (!strcmp(func, "apply_ssh_owner_nonce")) {
        workstate = J("state") ? json_copy(J("state")) : json_object();
        ret = ws_apply_ssh_owner_nonce(sp, workstate, J("nonce"));
        echo = json_object();
        static const char *const KEYS[] = {"_SSH_OWNER_NONCE", "_SSH_RUNTIME_PURELIB",
                                           "_SSH_RUNTIME_MARKER", NULL};
        for (size_t i = 0; KEYS[i]; i++) {
            const json_t *v = json_obj_get(workstate, KEYS[i]);
            json_set(echo, KEYS[i], v ? json_copy(v) : json_null());
        }
        echo_state = true;
    } else if (!strcmp(func, "require_token")) {
        req = ws_h_request(in);
        ret = ws_require_token(sp, req);
    } else if (!strcmp(func, "host_header_middleware")) {
        req = ws_h_request(in);
        const json_t *as = J("app_state");
        json_t *empty = json_object();
        ret = ws_host_header_middleware(sp, req, as ? as : empty);
        json_free(empty);
    } else if (!strcmp(func, "plugin_api_runtime_gate")) {
        req = ws_h_request(in);
        ret = ws_plugin_api_runtime_gate(sp, req);
    } else if (!strcmp(func, "dashboard_auth_gate")) {
        req = ws_h_request(in);
        ret = ws_dashboard_auth_gate(sp, req);
    } else if (!strcmp(func, "auth_middleware")) {
        req = ws_h_request(in);
        ret = ws_auth_middleware(sp, req);
    } else if (!strcmp(func, "token_auth_seam")) {
        req = ws_h_request(in);
        ret = ws_token_auth_seam(sp, req);
    } else if (!strcmp(func, "dashboard_health_middleware")) {
        req = ws_h_request(in);
        ret = ws_dashboard_health_middleware(sp, req);
    } else if (!strcmp(func, "record_selftest")) {
        workstate = ws_h_health(in);
        ret = ws_record_selftest(sp, workstate, ws_jtruth(J("passed")), J("http_status"));
        echo_state = true;
    } else if (!strcmp(func, "is_trusted_local_origin")) {
        const json_t *as = J("app_state");
        json_t *empty = json_object();
        ret = ws_is_trusted_local_origin(sp, as ? as : empty, json_get_str(in, "origin", ""));
        json_free(empty);
    } else if (!strcmp(func, "is_allowed_origin")) {
        const json_t *as = J("app_state");
        json_t *empty = json_object();
        ret = ws_is_allowed_origin(sp, as ? as : empty, json_get_str(in, "origin", ""));
        json_free(empty);
    } else if (!strcmp(func, "no_auth_provider_message")) {
        const json_t *as = J("app_state");
        json_t *empty = json_object();
        ret = ws_no_auth_provider_message(sp, as ? as : empty, json_get_str(in, "host", ""));
        json_free(empty);
    } else if (!strcmp(func, "configure_auth_gate")) {
        workstate = J("state") ? json_copy(J("state")) : json_object();
        ret = ws_configure_auth_gate(sp, workstate, json_get_str(in, "host", ""),
                                     ws_jtruth(J("allow_public")), J("ssh_session_token"),
                                     J("ssh_owner_nonce"));
        echo_state = true;
    } else if (!strcmp(func, "best_effort")) {
        ret = ws_best_effort(sp, json_get_str(in, "what", ""), J("fn"));
    } else if (!strcmp(func, "reclaim_host_from_orphaned_owner")) {
        ret = ws_reclaim_host_from_orphaned_owner(sp, json_get_str(in, "role", "serve"));
    } else if (!strcmp(func, "publish_host_rendezvous")) {
        workstate = J("state") ? json_copy(J("state")) : json_object();
        ret = ws_publish_host_rendezvous(sp, workstate, json_get_str(in, "host", ""),
                                         json_get_num(in, "port", 9119),
                                         json_get_str(in, "session_token", ""));
        echo_state = true;
    } else if (!strcmp(func, "windows_serve_loop_factory")) {
        ret = ws_windows_serve_loop_factory(sp);
    } else if (!strcmp(func, "run_serve")) {
        ret = ws_run_serve(sp, json_get_str(in, "platform", "linux"),
                           json_get_str(in, "host", "127.0.0.1"),
                           json_get_num(in, "port", 9119));
    } else if (!strcmp(func, "start_server")) {
        ret = ws_start_server(sp, J("args"), J("kwargs"));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    json_set(out, "ret", ws_norm_raise(ret ? ret : json_null()));
    json_set(out, "trace", trace.events ? json_copy(trace.events) : json_array());
    if (echo_state) {
        json_t *st = echo ? echo : workstate;
        json_set(out, "state", st ? json_copy(st) : json_object());
    }
    ws_trace_free(&trace);
    json_free(counters);
    if (req) json_free(req);
    if (workstate) json_free(workstate);
    if (echo) json_free(echo);
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
