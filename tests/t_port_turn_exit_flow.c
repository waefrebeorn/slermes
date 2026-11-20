/*
 * t_port_turn_exit_flow.c — oracle harness for the UNCLAIMED turn-exit
 * FLOW surface ported in src/cli/turn_exit_flow.c (the exit-policy chain
 * consuming hermes_cli/turn_exit.py, whose own two defs are fully claimed
 * by session_kanban_flow2 and SEAMED here).
 *
 * Self-contained: includes the port .c by area-qualified name. Reads
 * JSON-line cases from stdin (tests/fixtures/turn_exit_flow.jsonl), wires
 * the universal seam to the SCRIPTED results in each case's "seams" object
 * (a recording stub appending one trace event per seam call, in call
 * order), and emits {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}
 * (plus "state" for the mutation flows: sync_cli_session_id,
 * configure_quiet_agent, collect_kanban_task_images, finite_block,
 * exit_single_query, take_turn_report_path).
 *
 * FAIL-CLOSED: a seam key MISSING from the script delivers
 * {"__raise__":"RuntimeError","msg":"unscripted seam: <name>"}; a present
 * JSON null is a valid None result; a JSON array script is one result PER
 * CALL (last repeats, empty array -> None). tests/oracle_turn_exit_flow.py
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

#include "cli/turn_exit_flow.c"

typedef struct {
    const json_t *in;
    te2_trace_t *trace;
    json_t *counters;
} te2_script_ctx_t;

static json_t *te2_args_drop_null(const json_t *args) {
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

static json_t *te2_script_call(void *vctx, const char *name, const json_t *args) {
    te2_script_ctx_t *ctx = vctx;
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (!sc) {
        /* fail-closed: an unscripted seam raises RuntimeError on both sides */
        char *msg = te2_xsf("unscripted seam: %s", name);
        result = te2_raise_spec("RuntimeError", msg);
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
    json_set(e, "args", te2_args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    te2_trace_add(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define J(k) json_obj_get(in, k)

static json_t *copy_or(const json_t *v, json_t *dflt) {
    return v ? json_copy(v) : dflt;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    te2_trace_t trace = {NULL};
    json_t *counters = json_object();
    te2_script_ctx_t ctx = {in, &trace, counters};
    te2_seams_t seams = {&ctx, te2_script_call};
    const te2_seams_t *sp = &seams;
    json_t *ret = NULL;
    json_t *workstate = NULL;   /* mutation-flow working copy */
    json_t *env = NULL;         /* environ working copy */
    json_t *images = NULL;      /* collect images working copy */
    json_t *empty_obj = NULL;
    json_t *empty_arr = NULL;
    bool echo_state = false;

    if (!strcmp(func, "int_or")) {
        ret = te2_int_or(J("value"), J("default"));
    } else if (!strcmp(func, "single_query_exit_code")) {
        ret = te2_single_query_exit_code(sp, J("result"),
                                         json_get_bool(in, "credentials_rate_limited", false),
                                         json_get_bool(in, "credentials_terminal", false));
    } else if (!strcmp(func, "sync_cli_session_id")) {
        workstate = copy_or(J("cli"), json_object());
        ret = te2_sync_cli_session_id(workstate);
        echo_state = true;
    } else if (!strcmp(func, "interrupt_agent_for_signal")) {
        ret = te2_interrupt_agent_for_signal(sp, J("agent"),
                                             json_get_num(in, "signum", 0));
    } else if (!strcmp(func, "configure_quiet_agent")) {
        workstate = copy_or(J("agent"), json_object());
        ret = te2_configure_quiet_agent(workstate);
        echo_state = true;
    } else if (!strcmp(func, "run_kanban_goal_loop_chat")) {
        const json_t *cli = J("cli");
        if (!cli) { empty_obj = json_object(); cli = empty_obj; }
        ret = te2_run_kanban_goal_loop_chat(sp, cli,
                                            json_get_str(in, "first_response", ""));
    } else if (!strcmp(func, "collect_kanban_task_images")) {
        images = copy_or(J("images"), json_array());
        env = copy_or(J("environ"), json_object());
        ret = te2_collect_kanban_task_images(sp, images, env);
        workstate = json_object();
        json_set(workstate, "images", json_copy(images));
        json_set(workstate, "environ", json_copy(env));
        echo_state = true;
    } else if (!strcmp(func, "route_single_query_images")) {
        const json_t *cli = J("cli");
        if (!cli) { empty_obj = json_object(); cli = empty_obj; }
        const json_t *imgs = J("images");
        if (!imgs) { empty_arr = json_array(); imgs = empty_arr; }
        ret = te2_route_single_query_images(sp, cli, J("query"), J("effective_query"),
                                            imgs, J("image_urls"));
    } else if (!strcmp(func, "pending_preview")) {
        ret = te2_pending_preview(sp, J("text"));
    } else if (!strcmp(func, "quoted")) {
        const json_t *row = J("row");
        if (!row) { empty_obj = json_object(); row = empty_obj; }
        ret = te2_quoted(sp, row, json_get_str(in, "fallback", ""));
    } else if (!strcmp(func, "pending_lines")) {
        const json_t *pending = J("pending");
        if (!pending) { empty_arr = json_array(); pending = empty_arr; }
        ret = te2_pending_lines(sp, pending, json_get_bool(in, "interactive", true));
    } else if (!strcmp(func, "lost_admission")) {
        ret = te2_lost_admission(J("snapshot"), json_get_str(in, "admission_id", ""));
    } else if (!strcmp(func, "oneshot_admission")) {
        ret = te2_oneshot_admission(J("receipt"));
    } else if (!strcmp(func, "resume_footer")) {
        ret = te2_resume_footer(sp, json_get_str(in, "session_id", ""));
    } else if (!strcmp(func, "finite_block")) {
        workstate = copy_or(J("self"), json_object());
        ret = te2_finite_block(workstate, json_get_str(in, "admission", ""));
        echo_state = true;
    } else if (!strcmp(func, "exit_single_query")) {
        env = copy_or(J("environ"), json_object());
        ret = te2_exit_single_query(sp, env, json_get_num(in, "code", 0));
        workstate = json_object();
        json_set(workstate, "environ", json_copy(env));
        echo_state = true;
    } else if (!strcmp(func, "take_turn_report_path")) {
        env = copy_or(J("environ"), json_object());
        ret = te2_take_turn_report_path(env);
        workstate = json_object();
        json_set(workstate, "environ", json_copy(env));
        echo_state = true;
    } else if (!strcmp(func, "oneshot_exit_code")) {
        const json_t *result = J("result");
        if (!result) { empty_obj = json_object(); result = empty_obj; }
        ret = te2_oneshot_exit_code(J("response"), result);
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", trace.events ? json_copy(trace.events) : json_array());
    if (echo_state) json_set(out, "state", workstate ? workstate : json_object());
    te2_trace_free(&trace);
    json_free(counters);
    if (workstate && !echo_state) json_free(workstate);
    if (env) json_free(env);
    if (images) json_free(images);
    if (empty_obj) json_free(empty_obj);
    if (empty_arr) json_free(empty_arr);
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