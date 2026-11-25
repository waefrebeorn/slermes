/*
 * t_port_gateway_agent_cli_flow.c — oracle harness for the gateway agent-bridge
 * FLOW cores + leaves ported in src/cli/gateway_agent_cli_flow.c.
 *
 * Self-contained: includes the port .c by AREA-QUALIFIED name. Reads JSON-line
 * cases from stdin (tests/fixtures/gateway_agent_cli_flow.jsonl), wires the
 * universal seam to the SCRIPTED results in each case's "seams" object (a
 * recording stub appending one trace event per seam call, in call order), and
 * emits {"func":..., "in":..., "out":{"ret":..., "trace":[...], "st":...}}.
 * The injected self-model "st" is mutated in place and echoed back.
 *
 * Fail-closed: a missing or empty seam script yields a RuntimeError raise spec
 * (recorded as the event ret, then propagated/swallowed by the port exactly as
 * live). An uncaught raise parks in gac_pending and is emitted as ret.
 * tests/oracle_gateway_agent_cli_flow.py drives the LIVE Python with the same
 * scripts and compares the ordered decision sequence + return + mutated state.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/gateway_agent_cli_flow.c"

typedef struct {
    const json_t *in;
    json_t *trace;     /* array of events */
    json_t *counters;
} script_ctx_t;

static json_t *make_raise(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}

static json_t *args_drop_null(const json_t *args) {
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

static json_t *script_call(void *vctx, const char *name, const json_t *args) {
    script_ctx_t *ctx = vctx;
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (sc && sc->type == JSON_ARRAY) {
        size_t n = json_len(sc);
        if (n == 0) {
            char *m = xsf("no script for %s", name);
            result = make_raise("RuntimeError", m);
            free(m);
        } else {
            double idx = json_get_num(ctx->counters, name, 0);
            size_t i = (size_t)idx;
            result = json_copy(json_get(sc, i < n ? i : n - 1));
            json_set(ctx->counters, name, json_number(idx + 1));
        }
    } else if (sc) {
        result = json_copy(sc);
    } else {
        char *m = xsf("no script for %s", name);
        result = make_raise("RuntimeError", m);
        free(m);
    }
    /* A scripted JSON null is delivered as a NULL pointer, not a json_null node. */
    if (result && result->type == JSON_NULL) { json_free(result); result = NULL; }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    if (!ctx->trace) ctx->trace = json_array();
    json_append(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define SD(k, d) json_get_str(in, k, d)
#define J(k) json_obj_get(in, k)
#define B(k, d) json_get_bool(in, k, d)

static void ret_json(json_t *out, json_t *r) { json_set(out, "ret", r ? r : json_null()); }
static void ret_bool(json_t *out, bool b) { json_set(out, "ret", json_bool(b)); }
static void ret_str(json_t *out, char *s) {
    json_set(out, "ret", s ? json_string(s) : json_null());
    free(s);
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = {in, NULL, json_object()};
    gac_seams_t seams = {&ctx, script_call};
    const gac_seams_t *sp = &seams;
    json_t *st = json_has(in, "st") ? json_copy(J("st")) : NULL;
    gac_take_pending();  /* clear any stale parked raise */

    if (!strcmp(func, "review_summary_line")) {
        ret_str(out, gac_review_summary_line(J("payload")));
    } else if (!strcmp(func, "summarize_api_error")) {
        ret_str(out, gac_summarize_api_error(J("exc")));
    } else if (!strcmp(func, "rate_limit_state")) {
        ret_json(out, gac_rate_limit_state(J("data"), sp));
    } else if (!strcmp(func, "moa_handler")) {
        gac_moa_handler(SD("kind", "moa.reference"), J("payload"), B("cb_present", false), sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "apply_info")) {
        gac_apply_info(st, J("info"));
        ret_json(out, NULL);
    } else if (!strcmp(func, "tools")) {
        ret_json(out, gac_tools(st));
    } else if (!strcmp(func, "valid_tool_names")) {
        ret_json(out, gac_valid_tool_names(st));
    } else if (!strcmp(func, "apply_session_usage")) {
        gac_apply_session_usage(st, J("usage"), sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "reset_session_figures")) {
        gac_reset_session_figures(st);
        ret_json(out, NULL);
    } else if (!strcmp(func, "reset_session_state")) {
        gac_reset_session_state(st, sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "interrupt")) {
        gac_interrupt(st, S("message"), sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "hard_interrupt")) {
        gac_hard_interrupt(st, S("message"), sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "steer")) {
        ret_json(out, gac_steer(st, S("text"), sp));
    } else if (!strcmp(func, "redirect")) {
        ret_json(out, gac_redirect(st, S("text"), sp));
    } else if (!strcmp(func, "expired")) {
        ret_bool(out, gac_expired(st, sp));
    } else if (!strcmp(func, "stage_content_parts")) {
        ret_json(out, gac_stage_content_parts(st, J("parts"), sp));
    } else if (!strcmp(func, "lost_turn_notice")) {
        ret_str(out, gac_lost_turn_notice(st, sp));
    } else if (!strcmp(func, "user_row")) {
        ret_json(out, gac_user_row(st, J("user_message")));
    } else if (!strcmp(func, "failed")) {
        ret_json(out, gac_failed(st, J("history"), J("user_message"), S("error"),
                                 S("failure_reason"), B("retryable", false)));
    } else if (!strcmp(func, "unadmitted")) {
        ret_json(out, gac_unadmitted(st, J("history"), J("user_message"), S("error"), sp));
    } else if (!strcmp(func, "emit_print")) {
        gac_emit_print(st, SD("text", ""), sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "on_print")) {
        gac_on_print(st, J("payload"), sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "on_review_summary")) {
        gac_on_review_summary(st, J("payload"), sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "owns")) {
        ret_bool(out, gac_owns(st, J("turn")));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
        ret_json(out, NULL);
    }

    /* An uncaught raise overrides the normal return. */
    json_t *pending = gac_take_pending();
    if (pending) json_set(out, "ret", pending);

    json_set(out, "trace", ctx.trace ? ctx.trace : json_array());
    if (st) json_set(out, "st", st);
    json_free(ctx.counters);
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