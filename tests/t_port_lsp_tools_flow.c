/*
 * t_port_lsp_tools_flow.c — oracle harness for the LSP tools surface FLOW
 * ported in src/tools/lsp_tools_flow.c (tools/file_operations_lint.py LSP tier
 * + agent/lsp/deferred.py queue + agent/lsp/sync_diagnostics.py leaves).
 *
 * Self-contained: includes the port .c by area-qualified name. Reads JSON-line
 * cases from stdin (tests/fixtures/lsp_tools_flow.jsonl), wires the universal
 * seam to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...][, "state":...]}}.
 * "state" is echoed for the deferred-queue mutation flows.
 *
 * FAIL-CLOSED: a seam key MISSING from the script delivers
 * {"__raise__":"RuntimeError","msg":"unscripted seam: <name>"}; a present JSON
 * null is a valid None result; a JSON array script is one result PER CALL
 * (last repeats, empty array -> None). tests/oracle_lsp_tools_flow.py drives
 * the LIVE Python with the same scripts and compares the ordered decision
 * sequence + return value (+ mutated queue state).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "tools/lsp_tools_flow.c"

typedef struct {
    const json_t *in;
    json_t *trace;
    json_t *counters;
} lt_script_ctx_t;

static json_t *lt_args_drop_null(const json_t *args) {
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

static json_t *lt_script_call(void *vctx, const char *name, const json_t *args) {
    lt_script_ctx_t *ctx = vctx;
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (!sc) {
        char *msg = lt_xsf("unscripted seam: %s", name);
        result = lt_raise("RuntimeError", msg);
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
    json_set(e, "args", lt_args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    json_append(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define J(k) json_obj_get(in, k)

/* The oracle drive's BaseException catch builds exactly {"__raise__": cls,
 * "msg": str(exc)}. A propagated scripted raise may carry extra fields; strip
 * them at the ret boundary, keeping msg (== str(exc) for fixture raises). */
static json_t *lt_norm_raise_ret(json_t *ret) {
    if (!lt_raised(ret)) return ret;
    json_t *n = json_object();
    json_set(n, "__raise__", json_string(lt_kind(ret)));
    json_set(n, "msg", json_string(lt_emsg(ret)));
    json_free(ret);
    return n;
}

static json_t *lt_default_state(void) {
    json_t *st = json_object();
    json_set(st, "queued", json_object());
    json_set(st, "pending", json_object());
    json_set(st, "next_token", json_number(0));
    return st;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    json_t *trace = json_array();
    json_t *counters = json_object();
    lt_script_ctx_t ctx = {in, trace, counters};
    lt_seams_t seams = {&ctx, lt_script_call};
    const lt_seams_t *sp = &seams;
    json_t *ret = NULL;
    json_t *state = NULL;
    bool echo_state = false;

    /* materialize mutable queue state for the deferred flows */
    const json_t *st_in = J("state");
    state = (st_in && st_in->type == JSON_OBJECT) ? json_copy(st_in) : lt_default_state();
    if (!lt_oget(state, "queued")) json_set(state, "queued", json_object());
    if (!lt_oget(state, "pending")) json_set(state, "pending", json_object());
    if (!json_has(state, "next_token")) json_set(state, "next_token", json_number(0));

    if (!strcmp(func, "lsp_service")) {
        ret = lt_lsp_service(sp, J("self"));
    } else if (!strcmp(func, "lsp_call")) {
        ret = lt_lsp_call(sp, J("self"), json_get_str(in, "method", ""),
                          json_get_str(in, "path", ""), J("default"));
    } else if (!strcmp(func, "local_workspace_untrusted")) {
        ret = lt_local_workspace_untrusted(sp, J("self"));
    } else if (!strcmp(func, "run_managed_node_linter")) {
        ret = lt_run_managed_node_linter(sp, J("self"), json_get_str(in, "ext", ""),
                                         json_get_str(in, "path", ""),
                                         json_get_bool(in, "is_windows", false));
    } else if (!strcmp(func, "format_lsp_block")) {
        ret = lt_format_lsp_block(sp, json_get_str(in, "path", ""), J("diagnostics"),
                                  json_get_str(in, "header", ""));
    } else if (!strcmp(func, "post_deferred")) {
        ret = lt_post_deferred(sp, state, json_get_str(in, "owner", ""),
                               json_get_str(in, "abs_path", ""), J("future"), J("carry"), J("token"));
        echo_state = true;
    } else if (!strcmp(func, "queue_late_verdict")) {
        ret = lt_queue_late_verdict(sp, state, json_get_str(in, "owner", ""),
                                    json_get_str(in, "abs_path", ""), J("future"), J("carry"));
        echo_state = true;
    } else if (!strcmp(func, "bind_owner")) {
        ret = lt_bind_owner(sp, S("owner"), J("body"));
    } else if (!strcmp(func, "current_owner")) {
        ret = lt_current_owner(sp);
    } else if (!strcmp(func, "deferred_post")) {
        lt_deferred_post(state, json_get_str(in, "owner", ""), json_get_str(in, "path", ""),
                         J("text"), J("baseline_text"));
        ret = NULL;
        echo_state = true;
    } else if (!strcmp(func, "deferred_take")) {
        ret = lt_deferred_take(state, S("owner"), json_get_str(in, "path", ""));
        echo_state = true;
    } else if (!strcmp(func, "deferred_restore")) {
        ret = lt_deferred_restore(state, S("owner"), json_get_str(in, "path", ""), J("notes"));
        echo_state = true;
    } else if (!strcmp(func, "pop_notes")) {
        ret = lt_deferred_pop_notes(state, J("owners"));
        echo_state = true;
    } else if (!strcmp(func, "track_pending")) {
        ret = lt_deferred_track_pending(state, json_get_str(in, "owner", ""));
        echo_state = true;
    } else if (!strcmp(func, "settled")) {
        ret = lt_deferred_settled(sp, state, json_get_str(in, "owner", ""), J("token"));
        echo_state = true;
    } else if (!strcmp(func, "owners")) {
        ret = lt_owners(J("agent"));
    } else if (!strcmp(func, "settle_for_agent")) {
        ret = lt_settle_for_agent(sp, state, J("agent"), json_get_num(in, "timeout", 5.0));
        echo_state = true;
    } else if (!strcmp(func, "forward_to_agent")) {
        ret = lt_forward_to_agent(sp, state, J("agent"));
        echo_state = true;
    } else if (!strcmp(func, "sync_supported")) {
        ret = lt_sync_supported(J("client"));
    } else if (!strcmp(func, "from_tsserver")) {
        ret = lt_from_tsserver(J("item"));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    ret = lt_norm_raise_ret(ret);
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", json_copy(trace));
    if (echo_state) {
        /* pending sets echo SORTED (order-free); queued preserves insertion order */
        json_t *pending = (json_t *)lt_oget(state, "pending");
        if (pending && pending->type == JSON_OBJECT) {
            for (size_t i = 0; i < pending->c.count; i++) {
                json_t *arr = pending->c.items[i];
                if (arr && arr->type == JSON_ARRAY) {
                    for (size_t a = 0; a + 1 < arr->c.count; a++)
                        for (size_t b = a + 1; b < arr->c.count; b++) {
                            json_t *x = arr->c.items[a], *y = arr->c.items[b];
                            if (x && y && x->type == JSON_NUMBER && y->type == JSON_NUMBER &&
                                x->num_val > y->num_val) {
                                arr->c.items[a] = y;
                                arr->c.items[b] = x;
                            }
                        }
                }
            }
        }
        json_set(out, "state", json_copy(state));
    }
    json_free(trace);
    json_free(counters);
    json_free(state);
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
