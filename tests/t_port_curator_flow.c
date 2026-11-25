/*
 * t_port_curator_flow.c — oracle harness for the UNCLAIMED hermes_cli/curator.py
 * FLOW cores + leaves ported in src/cli/curator_flow.c.
 *
 * Self-contained: includes the port .c by AREA-QUALIFIED name. Reads JSON-line
 * cases from stdin (tests/fixtures/curator_flow.jsonl), wires the universal seam
 * to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 *
 * Fail-closed: a missing or empty seam script yields a RuntimeError raise spec
 * (recorded as the event ret, then propagated/swallowed by the port exactly as
 * live). A scripted JSON null is delivered as a NULL pointer (Python None).
 * tests/oracle_curator_flow.py drives the LIVE Python with the same scripts and
 * compares the ordered decision sequence + return.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/curator_flow.c"

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
            char *m = cur_fmt("no script for %s", name);
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
        char *m = cur_fmt("no script for %s", name);
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
    cur_io_t io = {script_call, &ctx, J("env")};
    json_t *r = NULL;

    if (!strcmp(func, "parse_ts")) {
        r = cur_parse_ts(J("ts"));
    } else if (!strcmp(func, "arg")) {
        r = cur_arg(J("flags"), J("kwargs"));
    } else if (!strcmp(func, "freq")) {
        r = cur_freq(J("r"));
    } else if (!strcmp(func, "usage_key_name")) {
        r = cur_usage_key_name(J("r"));
    } else if (!strcmp(func, "usage_key_recent")) {
        r = cur_usage_key_recent(J("r"));
    } else if (!strcmp(func, "usage_key_activity")) {
        r = cur_usage_key_activity(J("r"));
    } else if (!strcmp(func, "report_rollback")) {
        r = cur_report_rollback(&io, B("ok", false), SD("msg", ""));
    } else if (!strcmp(func, "confirm")) {
        r = cur_confirm(&io, SD("prompt", ""), SD("cancel", "cancelled"), SD("eof_prefix", "\n"));
    } else if (!strcmp(func, "print_skill_rows")) {
        r = cur_print_skill_rows(&io, SD("title", ""), J("rows"));
    } else if (!strcmp(func, "print_curator_config")) {
        r = cur_print_curator_config(&io);
    } else if (!strcmp(func, "set_paused")) {
        r = cur_set_paused(&io, B("paused", false));
    } else if (!strcmp(func, "set_pin")) {
        r = cur_set_pin(&io, SD("skill", ""), B("pinned", true));
    } else if (!strcmp(func, "set_lock")) {
        r = cur_set_lock(&io, SD("skill", ""), B("locked", true));
    } else if (!strcmp(func, "cmd_lock")) {
        r = cur_cmd_lock(&io, SD("skill", ""));
    } else if (!strcmp(func, "cmd_unlock")) {
        r = cur_cmd_unlock(&io, SD("skill", ""));
    } else if (!strcmp(func, "as_user")) {
        r = cur_as_user(&io, SD("skill", ""));
    } else if (!strcmp(func, "cmd_ledger")) {
        r = cur_cmd_ledger(&io, B("compact", false), SD("skill", NULL), J("limit"));
    } else if (!strcmp(func, "cmd_purge")) {
        r = cur_cmd_purge(&io, J("days"), B("dry_run", false), B("yes", false));
    } else if (!strcmp(func, "rollback_ledger_entry")) {
        r = cur_rollback_ledger_entry(&io, B("yes", false), SD("entry_id", ""));
    } else if (!strcmp(func, "archived_ts")) {
        r = cur_archived_ts(&io, SD("path", ""), J("usage"));
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