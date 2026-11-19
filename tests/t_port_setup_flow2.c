/*
 * t_port_setup_flow2.c — oracle harness for the unclaimed hermes_cli/setup.py
 * FLOW cores + leaves ported in src/cli/setup_flow2.c.
 *
 * Self-contained: includes the port .c by AREA-QUALIFIED name. Reads JSON-line
 * cases from stdin (tests/fixtures/setup_flow2.jsonl), wires the universal seam
 * to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...], "state":{...}}}.
 * Injected dicts (parent / section / config / answers / nav) are mutated in
 * place and echoed back in out.state.
 *
 * Fail-closed: a missing or empty seam script yields a RuntimeError raise spec
 * (recorded as the event ret, then propagated/swallowed by the port exactly as
 * live). A scripted JSON null is delivered as a NULL pointer (Python None).
 * tests/oracle_setup_flow2.py drives the LIVE Python with the same scripts and
 * compares the ordered decision sequence + return + mutated state.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/setup_flow2.c"

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
            char *m = st2_fmt("no script for %s", name);
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
        char *m = st2_fmt("no script for %s", name);
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

static json_t *copy_obj_field(const json_t *in, const char *key) {
    const json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_OBJECT) ? json_copy(v) : json_object();
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = {in, NULL, json_object()};
    st2_io_t io = {script_call, &ctx, J("env")};
    json_t *state = NULL;
    json_t *r = NULL;

    if (!strcmp(func, "info")) {
        r = st2_info(&io, J("lines"));
    } else if (!strcmp(func, "sub_dict")) {
        json_t *parent = copy_obj_field(in, "parent");
        r = st2_sub_dict(parent, SD("key", ""));
        state = json_object();
        json_set(state, "parent", parent);
    } else if (!strcmp(func, "section_rule")) {
        r = st2_section_rule(&io, SD("title", ""));
    } else if (!strcmp(func, "print_banner")) {
        r = st2_print_banner(&io, J("lines"));
    } else if (!strcmp(func, "module_installed")) {
        r = st2_module_installed(&io, SD("name", ""));
    } else if (!strcmp(func, "prompt_number")) {
        r = st2_prompt_number(&io, SD("label", ""), J("current"), SD("cast", "int"));
    } else if (!strcmp(func, "prompt_int_setting")) {
        json_t *section = copy_obj_field(in, "section");
        r = st2_prompt_int_setting(&io, section, SD("key", ""), SD("label", ""), J("current"));
        state = json_object();
        json_set(state, "section", section);
    } else if (!strcmp(func, "prompt_and_save_env_var")) {
        r = st2_prompt_and_save_env_var(&io, J("var"), SD("saved_msg", ""), SD("skipped_msg", ""));
    } else if (!strcmp(func, "record_setup_completed")) {
        json_t *config = copy_obj_field(in, "config");
        r = st2_record_setup_completed(&io, config);
        state = json_object();
        json_set(state, "config", config);
    } else if (!strcmp(func, "run_setup_wizard")) {
        r = st2_run_setup_wizard(&io, J("args"));
    } else if (!strcmp(func, "run_setup_section")) {
        json_t *config = copy_obj_field(in, "config");
        r = st2_run_setup_section(&io, config, J("section"));
        state = json_object();
        json_set(state, "config", config);
    } else if (!strcmp(func, "run_full_setup")) {
        json_t *config = copy_obj_field(in, "config");
        r = st2_run_full_setup(&io, config, SD("hermes_home", ""),
                               B("is_existing", false), SD("imported_from", ""));
        state = json_object();
        json_set(state, "config", config);
    } else if (!strcmp(func, "run_setup_wizard_impl")) {
        r = st2_run_setup_wizard_impl(&io, J("args"));
    } else if (!strcmp(func, "nav_state_init")) {
        st2_nav_t st = {0, 0, 0, NULL, NULL};
        st2_nav_state_init(&st, (int)N("section_index", -1.0), (int)N("prompt_index", 0.0));
        state = json_object();
        json_set(state, "nav", st2_nav_to_json(&st));
        st2_nav_free(&st);
    } else if (!strcmp(func, "nav_state_reset")) {
        st2_nav_t st = {0, 0, 0, NULL, NULL};
        st2_nav_load(J("nav"), &st);
        st2_nav_state_reset(&st, (int)N("section_index", -1.0), J("replay"));
        state = json_object();
        json_set(state, "nav", st2_nav_to_json(&st));
        st2_nav_free(&st);
    } else if (!strcmp(func, "go_back_init")) {
        r = st2_go_back_init((int)N("prompt_index", 0.0));
    } else if (!strcmp(func, "record_answers")) {
        const json_t *nj = J("nav");
        st2_nav_t st = {0, 0, 0, NULL, NULL};
        st2_nav_t *stp = NULL;
        if (nj && nj->type == JSON_OBJECT) { st2_nav_load(nj, &st); stp = &st; }
        json_t *answers = copy_obj_field(in, "answers");
        st2_record_answers(stp, (int)N("section_index", 0.0), answers);
        state = json_object();
        json_set(state, "answers", answers);
        json_set(state, "nav", stp ? st2_nav_to_json(&st) : json_null());
        if (stp) st2_nav_free(&st);
    } else if (!strcmp(func, "skip")) {
        r = st2_skip(&io, J("config"), SD("imported_from", ""), SD("key", ""), SD("label", ""));
    } else if (!strcmp(func, "gateway_step")) {
        json_t *config = copy_obj_field(in, "config");
        r = st2_gateway_step(&io, config, SD("imported_from", ""));
        state = json_object();
        json_set(state, "config", config);
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
        r = make_raise("RuntimeError", "unknown func");
    }

    ret_json(out, r);
    json_set(out, "trace", ctx.trace ? ctx.trace : json_array());
    if (state) json_set(out, "state", state);
    else json_free(state);
    json_free(ctx.counters);
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
        /* json_escape_string has a 1-byte heap overflow (need=1, writes 2
         * quotes + NUL); the bounds-safe serializer emits identical output. */
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