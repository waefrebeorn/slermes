/*
 * t_port_web_profiles_flow3.c — oracle harness for the UNCLAIMED async endpoint
 * + sidebar-cache writers of hermes_cli/web_routers/profiles.py ported in
 * src/cli/web_profiles_flow3.c.
 *
 * Self-contained: includes the port .c by area-qualified name. Reads one JSON
 * case per line from stdin (tests/fixtures/web_profiles_flow3.jsonl), wires the
 * universal seam to the SCRIPTED results in each case's "seams" object (a
 * recording stub appending one trace event per seam call, in call order), and
 * emits {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 *
 * FAIL-CLOSED: a seam key MISSING from the script delivers
 * {"__raise__":"RuntimeError","msg":"unscripted seam: <name>"}; a present JSON
 * null is a valid None; a JSON array script is one result PER CALL (last
 * repeats, empty -> None). tests/oracle_web_profiles_flow3.py drives the LIVE
 * Python with the same scripts and compares the ordered trace + return.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cli/web_profiles_flow3.c"

typedef struct {
    const json_t *in;
    json_t *idx;
    json_t *trace;
} wp3_script_ctx_t;

/* Mirror of the oracle's seam(): drop null args, SNAPSHOT args and ret so a
 * later in-place mutation cannot leak backwards into a recorded trace entry. */
static json_t *wp3_args_drop_null(const json_t *args) {
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

static json_t *wp3_script_call(void *vctx, const char *name, const json_t *args) {
    wp3_script_ctx_t *ctx = (wp3_script_ctx_t *)vctx;
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (!sc) {
        char *msg = wp3_xsf("unscripted seam: %s", name);
        result = wp3_raise_spec("RuntimeError", msg);
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
    json_set(e, "args", wp3_args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    json_append(ctx->trace, e);
    return result;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    wp3_script_ctx_t ctx = { in, json_object(), json_array() };
    wp3_seams_t seams = { &ctx, wp3_script_call };
    const wp3_seams_t *sp = &seams;
    json_t *ret = NULL;

    if (!strcmp(func, "profile_targets")) {
        ret = wp3_profile_targets(sp, json_get_str(in, "log_label", ""));
    } else if (!strcmp(func, "sidebar_cache_put")) {
        ret = wp3_sidebar_cache_put(sp, json_obj_get(in, "key"), json_obj_get(in, "value"),
                                    json_obj_get(in, "cache"),
                                    (long)json_get_num(in, "max_entries", 256));
    } else if (!strcmp(func, "sidebar_cache_clear")) {
        ret = wp3_sidebar_cache_clear(sp, json_obj_get(in, "cache"));
    } else if (!strcmp(func, "profile_errors")) {
        ret = wp3_profile_errors(sp, json_obj_get(in, "raise"), json_get_str(in, "log_msg", ""),
                                 json_obj_get(in, "args"), json_obj_get(in, "not_found"),
                                 json_obj_get(in, "bad_request"));
    } else if (!strcmp(func, "read_off_loop")) {
        ret = wp3_read_off_loop(sp, json_get_str(in, "label", ""), json_obj_get(in, "errors"),
                                "read_result");
    } else if (!strcmp(func, "profile_setup_command")) {
        ret = wp3_get_profile_setup_command(sp, json_get_str(in, "name", ""));
    } else if (!strcmp(func, "active_profile")) {
        ret = wp3_get_active_profile(sp);
    } else if (!strcmp(func, "set_active_profile")) {
        ret = wp3_set_active_profile(sp, json_get_str(in, "name", ""));
    } else if (!strcmp(func, "rename_profile")) {
        ret = wp3_rename_profile(sp, json_get_str(in, "name", ""), json_get_str(in, "new_name", ""));
    } else if (!strcmp(func, "delete_profile")) {
        ret = wp3_delete_profile(sp, json_get_str(in, "name", ""));
    } else if (!strcmp(func, "profile_soul")) {
        ret = wp3_get_profile_soul(sp, json_get_str(in, "name", ""));
    } else if (!strcmp(func, "update_soul")) {
        ret = wp3_update_profile_soul(sp, json_get_str(in, "name", ""), json_get_str(in, "content", ""));
    } else if (!strcmp(func, "update_model")) {
        ret = wp3_update_profile_model(sp, json_get_str(in, "name", ""),
                                       json_get_str(in, "provider", ""), json_get_str(in, "model", ""));
    } else if (!strcmp(func, "describe_auto")) {
        ret = wp3_describe_profile_auto(sp, json_get_str(in, "name", ""),
                                        wp3_jtruth(json_obj_get(in, "overwrite")));
    } else if (!strcmp(func, "create_profile")) {
        ret = wp3_create_profile(sp, json_obj_get(in, "body"));
    } else if (!strcmp(func, "open_terminal")) {
        ret = wp3_open_profile_terminal(sp, json_get_str(in, "name", ""), json_get_str(in, "platform", "linux"));
    } else if (!strcmp(func, "read_profiles")) {
        ret = wp3_read_profiles(sp);
    } else if (!strcmp(func, "list_profiles_endpoint")) {
        ret = wp3_list_profiles_endpoint(sp);
    } else if (!strcmp(func, "read_desktop_overlay")) {
        ret = wp3_read_desktop_overlay(sp, json_get_str(in, "profile_dir", ""));
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
