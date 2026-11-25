/*
 * t_port_kanban_cli_flow.c — oracle harness for the UNCLAIMED
 * hermes_cli/kanban.py FLOW cores + leaves ported in src/cli/kanban_cli_flow.c.
 *
 * Self-contained: includes the port .c by AREA-QUALIFIED name. Reads JSON-line
 * cases from stdin (tests/fixtures/kanban_cli_flow.jsonl), wires the universal
 * seam to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 *
 * Fail-closed: a missing or empty seam script yields a RuntimeError raise spec
 * (recorded as the event ret, then propagated/swallowed by the port exactly as
 * live). A scripted JSON null is delivered as a NULL pointer (Python None).
 * tests/oracle_kanban_cli_flow.py drives the LIVE Python with the same scripts
 * and compares the ordered seam trace + return.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/kanban_cli_flow.c"

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
            char *m = kc_fmt("no script for %s", name);
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
        char *m = kc_fmt("no script for %s", name);
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
#define N(k, d) json_get_num(in, k, d)

static void ret_json(json_t *out, json_t *r) { json_set(out, "ret", r ? r : json_null()); }

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = {in, NULL, json_object()};
    kc_io_t io = {script_call, &ctx};
    json_t *r = NULL;

    if (!strcmp(func, "none_profile")) {
        r = kc_none_profile(J("value"));
    } else if (!strcmp(func, "parse_metadata_flag")) {
        r = kc_parse_metadata_flag(&io, J("raw"));
    } else if (!strcmp(func, "joined_words")) {
        r = kc_joined_words(J("words"));
    } else if (!strcmp(func, "stripped_or_none")) {
        r = kc_stripped_or_none(J("value"));
    } else if (!strcmp(func, "ok_or_err")) {
        r = kc_ok_or_err(&io, J("ok"), SD("fail", ""), SD("done", ""));
    } else if (!strcmp(func, "bulk_ids")) {
        r = kc_bulk_ids(J("args"));
    } else if (!strcmp(func, "require_ids")) {
        r = kc_require_ids(&io, J("args"));
    } else if (!strcmp(func, "print_diagnostics")) {
        r = kc_print_diagnostics(&io, J("diags"), SD("indent", ""), B("with_kind", false));
    } else if (!strcmp(func, "print_section")) {
        r = kc_print_section(&io, SD("title", ""), J("lines"));
    } else if (!strcmp(func, "rows_by_task")) {
        r = kc_rows_by_task(&io, J("conn"), SD("table", ""), J("ids"));
    } else if (!strcmp(func, "goal_gate_error")) {
        r = kc_goal_gate_error(&io, J("conn"), SD("tid", ""), SD("evidence", ""),
                               SD("handoff", ""), SD("blocked_hint", ""),
                               SD("continue_hint", ""));
    } else if (!strcmp(func, "commented")) {
        json_t *closure = kc_commented(J("conn"), J("reason"), J("author"), SD("prefix", ""));
        if (kc_is_raise(closure)) {
            r = closure;
        } else {
            const json_t *tids = J("tids");
            if (tids && tids->type == JSON_ARRAY && json_len(tids) > 0) {
                for (size_t i = 0; i < json_len(tids); i++) {
                    json_free(r);
                    r = kc_commented_run(&io, closure, kc_jstr(json_get(tids, i)));
                    if (kc_is_raise(r)) break;
                }
            } else {
                r = kc_commented_run(&io, closure, SD("tid", ""));
            }
            json_free(closure);
        }
    } else if (!strcmp(func, "caller_identity_for_target")) {
        r = kc_caller_identity_for_target(&io, J("platform"), J("chat_id"),
                                          J("thread_id"), J("user_id"), J("user_id_alt"));
    } else if (!strcmp(func, "run_triage_sweep")) {
        r = kc_run_triage_sweep(&io, J("args"), SD("verb", ""), SD("json_key", ""),
                                J("json_fields"));
    } else if (!strcmp(func, "retitled_suffix")) {
        r = kc_retitled_suffix(J("outcome"));
    } else if (!strcmp(func, "decompose_ok_line")) {
        r = kc_decompose_ok_line(J("outcome"));
    } else if (!strcmp(func, "cmd_log")) {
        r = kc_cmd_log(&io, J("args"));
    } else if (!strcmp(func, "cmd_request_changes")) {
        r = kc_cmd_request_changes(&io, J("args"));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
        r = make_raise("RuntimeError", "unknown func");
    }

    ret_json(out, r);
    json_set(out, "trace", ctx.trace ? ctx.trace : json_array());
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
        json_t *func_node = json_string(func);
        char *func_s = json_serialize(func_node);
        json_free(func_node);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", func_s,
               in_s ? in_s : "null", out_s ? out_s : "null");
        free(in_s); free(out_s); free(func_s);
        json_free(out);
        json_free(c);
    }
    free(line);
    return 0;
}
