/*
 * t_port_web_profiles_flow2.c — oracle harness for the UNCLAIMED helpers of
 * hermes_cli/web_routers/profiles.py ported in src/cli/web_profiles_flow2.c.
 *
 * Self-contained: includes the port .c by area-qualified name. Reads one JSON
 * case per line from stdin (tests/fixtures/web_profiles_flow2.jsonl), wires the
 * universal seam to the SCRIPTED results in each case's "seams" object (a
 * recording stub appending one trace event per seam call, in call order), and
 * emits {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 *
 * FAIL-CLOSED: a seam key MISSING from the script delivers
 * {"__raise__":"RuntimeError","msg":"unscripted seam: <name>"}; a present JSON
 * null is a valid None; a JSON array script is one result PER CALL (last
 * repeats, empty -> None). tests/oracle_web_profiles_flow2.py drives the LIVE
 * Python with the same scripts and compares the ordered trace + return.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cli/web_profiles_flow2.c"

typedef struct {
    const json_t *in;
    json_t *idx;
    json_t *trace;
} wp2_script_ctx_t;

/* Mirror of the oracle's seam(): drop null args, SNAPSHOT args and ret so a
 * later in-place mutation cannot leak backwards into a recorded trace entry. */
static json_t *wp2_args_drop_null(const json_t *args) {
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

static json_t *wp2_script_call(void *vctx, const char *name, const json_t *args) {
    wp2_script_ctx_t *ctx = (wp2_script_ctx_t *)vctx;
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (!sc) {
        char *msg = wp2_xsf("unscripted seam: %s", name);
        result = wp2_raise_spec("RuntimeError", msg);
        free(msg);
    } else if (sc->type == JSON_OBJECT && json_obj_get(sc, "__arr__")) {
        result = json_copy(json_obj_get(sc, "__arr__"));
    } else if (sc->type == JSON_ARRAY) {
        double idx = json_get_num(ctx->idx, name, 0);
        size_t i = (size_t)idx;
        size_t n = json_len(sc);
        if (n > 0) result = json_copy(json_get(sc, i < n ? i : n - 1));
        json_set(ctx->idx, name, json_number(idx + 1));
    } else {
        result = json_copy(sc);
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", wp2_args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    json_append(ctx->trace, e);
    return result;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    wp2_script_ctx_t ctx = { in, json_object(), json_array() };
    wp2_seams_t seams = { &ctx, wp2_script_call };
    const wp2_seams_t *sp = &seams;
    json_t *ret = NULL;

    if (!strcmp(func, "recency")) {
        ret = wp2_recency(json_obj_get(in, "s"));
    } else if (!strcmp(func, "csv_list")) {
        ret = wp2_csv_list(json_obj_get(in, "value"));
    } else if (!strcmp(func, "slice_has_rows")) {
        ret = wp2_slice_has_rows(json_obj_get(in, "slices"));
    } else if (!strcmp(func, "failed_load_slice")) {
        ret = wp2_failed_load_slice(json_obj_get(in, "errors"), json_obj_get(in, "extra"));
    } else if (!strcmp(func, "profiles_failed")) {
        ret = wp2_profiles_failed(json_obj_get(in, "errors"));
    } else if (!strcmp(func, "no_store_file")) {
        ret = wp2_no_store_file(json_get_str(in, "name", ""), json_obj_get(in, "errors"));
    } else if (!strcmp(func, "tag_rows")) {
        ret = wp2_tag_rows(json_obj_get(in, "rows"), json_get_str(in, "name", ""),
                           json_get_num(in, "now", 0));
    } else if (!strcmp(func, "pinned_window")) {
        ret = wp2_pinned_window(json_obj_get(in, "rows"),
                                (long)json_get_num(in, "offset", 0),
                                (long)json_get_num(in, "cap", 0));
    } else if (!strcmp(func, "linux_terminal_commands")) {
        ret = wp2_linux_terminal_commands(json_get_str(in, "command", ""));
    } else if (!strcmp(func, "profile_state_db")) {
        ret = wp2_profile_state_db(sp, json_get_str(in, "home", ""));
    } else if (!strcmp(func, "corrupt_profile_stores")) {
        ret = wp2_corrupt_profile_stores(sp, json_obj_get(in, "targets"));
    } else if (!strcmp(func, "retryable_profile_errors")) {
        ret = wp2_retryable_profile_errors(sp, json_obj_get(in, "errors"),
                                           json_obj_get(in, "scanned"));
    } else if (!strcmp(func, "scoped_profile_targets")) {
        ret = wp2_scoped_profile_targets(sp, json_get_str(in, "log_label", ""),
                                         json_obj_get(in, "profile") ?
                                         json_get_str(in, "profile", "") : NULL);
    } else if (!strcmp(func, "best_effort")) {
        ret = wp2_best_effort(sp, json_get_str(in, "log_msg", ""),
                              json_obj_get(in, "args"), json_obj_get(in, "default"));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", ctx.trace ? json_copy(ctx.trace) : json_array());
    json_free(ctx.trace);
    json_free(ctx.idx);
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
