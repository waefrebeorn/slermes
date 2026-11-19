/*
 * t_port_cli_py_flow.c -- oracle harness for the unclaimed quarry ROOT cli.py
 * FLOW cores + leaves ported in src/cli/cli_py_flow.c.
 *
 * Self-contained: includes the port .c by AREA-QUALIFIED name. Reads JSON-line
 * cases from stdin (tests/fixtures/cli_py_flow.jsonl), wires the universal seam
 * to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...], "state":{...}}}.
 * Injected self/state dicts are mutated in place and echoed back in out.state
 * (presence-aware key sets, documented per func in the oracle).
 *
 * Fail-closed: a missing or empty seam script yields a RuntimeError raise spec
 * (recorded as the event ret, then propagated/swallowed by the port exactly as
 * live). A scripted JSON null is delivered as a NULL pointer (Python None).
 * tests/oracle_cli_py_flow.py drives the LIVE Python with the same scripts and
 * compares the ordered decision sequence + return + mutated state.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/cli_py_flow.c"

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
            char *m = cpy_fmt("no script for %s", name);
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
        char *m = cpy_fmt("no script for %s", name);
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

#define J(k) json_obj_get(in, k)
#define B(k, d) json_get_bool(in, k, d)

static void ret_json(json_t *out, json_t *r) { json_set(out, "ret", r ? r : json_null()); }

static json_t *copy_obj_field(const json_t *in, const char *key) {
    const json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_OBJECT) ? json_copy(v) : json_object();
}

/* Module-global defaults (the oracle seeds the same from setup_module_state):
 * absent state keys read as the live module globals' initial values. */
static void fill_module_state(json_t *st) {
    if (!cpy_has(st, "enabled")) json_set(st, "enabled", json_bool(true));
    if (!cpy_has(st, "max_lines")) json_set(st, "max_lines", json_number(200));
    if (!cpy_has(st, "replaying")) json_set(st, "replaying", json_bool(false));
    if (!cpy_has(st, "suppressed")) json_set(st, "suppressed", json_bool(false));
    if (!cpy_has(st, "history")) json_set(st, "history", json_array());
    if (!cpy_has(st, "skill_commands")) json_set(st, "skill_commands", json_null());
}

/* presence-aware echo of the self-state keys the oracle also echoes */
static json_t *echo_keys(const json_t *st, const char *const *keys, size_t n) {
    json_t *s = json_object();
    for (size_t i = 0; i < n; i++) {
        const json_t *v = cpy_oget(st, keys[i]);
        if (v) json_set(s, keys[i], json_copy(v));
    }
    return s;
}

static json_t *echo_history_state(const json_t *st) {
    json_t *s = json_object();
    const json_t *e = cpy_oget(st, "enabled");
    json_set(s, "enabled", e ? json_copy(e) : json_null());
    const json_t *m = cpy_oget(st, "max_lines");
    json_set(s, "max_lines", m ? json_copy(m) : json_null());
    const json_t *h = cpy_oget(st, "history");
    json_set(s, "history", h ? json_copy(h) : json_null());
    return s;
}

static json_t *echo_replay_state(const json_t *st) {
    json_t *s = json_object();
    const json_t *e = cpy_oget(st, "enabled");
    json_set(s, "enabled", e ? json_copy(e) : json_null());
    const json_t *rp = cpy_oget(st, "replaying");
    json_set(s, "replaying", rp ? json_copy(rp) : json_null());
    const json_t *h = cpy_oget(st, "history");
    json_set(s, "history", h ? json_copy(h) : json_null());
    return s;
}

static json_t *echo_skill_cache(const json_t *st) {
    json_t *s = json_object();
    const json_t *v = cpy_oget(st, "skill_commands");
    json_set(s, "skill_commands", v ? json_copy(v) : json_null());
    return s;
}

static const char *const CB_KEYS[] = { "_tool_callbacks_installed" };
static const char *const FIN_KEYS[] = {
    "_preload_skills_finalized", "preloaded_skills", "system_prompt",
};
static const char *const PI_KEYS[] = { "_pending_input" };

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = {in, NULL, json_object()};
    cpy_io_t io = {script_call, &ctx, J("env")};
    json_t *state = NULL;
    json_t *st = NULL;
    json_t *r = NULL;

    if (!strcmp(func, "configure_output_history")) {
        st = copy_obj_field(in, "state");
        fill_module_state(st);
        r = cpy_configure_output_history(&io, st, J("enabled"), J("max_lines"));
        state = echo_history_state(st);
    } else if (!strcmp(func, "suspend_output_history")) {
        st = copy_obj_field(in, "state");
        fill_module_state(st);
        json_t *old = cpy_suspend_output_history_enter(st);
        bool oldb = old && old->type == JSON_BOOL && old->bool_val;
        json_free(old);
        const json_t *sup = cpy_oget(st, "suppressed");
        bool during = cpy_truthy(sup);
        json_t *body = cpy_call(&io, "suspend_body", json_object());
        cpy_suspend_output_history_exit(st, oldb);
        sup = cpy_oget(st, "suppressed");
        state = json_object();
        json_set(state, "suppressed_during", json_bool(during));
        json_set(state, "suppressed_after", json_bool(cpy_truthy(sup)));
        r = body;   /* the CM restores state even when the body raised */
    } else if (!strcmp(func, "replay_output_history")) {
        st = copy_obj_field(in, "state");
        fill_module_state(st);
        r = cpy_replay_output_history(&io, st, J("fit"), J("output"));
        state = echo_replay_state(st);
    } else if (!strcmp(func, "looks_like_slash_command")) {
        r = cpy_looks_like_slash_command(J("text"));
    } else if (!strcmp(func, "slash_args")) {
        r = cpy_slash_args(J("cmd"));
    } else if (!strcmp(func, "ensure_skill_commands")) {
        st = copy_obj_field(in, "state");
        fill_module_state(st);
        r = cpy_ensure_skill_commands(&io, st);
        state = echo_skill_cache(st);
    } else if (!strcmp(func, "session_skill_commands")) {
        st = copy_obj_field(in, "state");
        fill_module_state(st);
        r = cpy_session_skill_commands(&io, st);
        state = echo_skill_cache(st);
    } else if (!strcmp(func, "should_seed_interactive")) {
        r = cpy_should_seed_interactive(&io, J("query"), J("image"),
                                        B("quiet", false), B("oneshot", false));
    } else if (!strcmp(func, "voice_input_message_str")) {
        st = copy_obj_field(in, "self");
        r = cpy_voice_input_message_str(st);
    } else if (!strcmp(func, "seeded_query_message_str")) {
        st = copy_obj_field(in, "self");
        r = cpy_seeded_query_message_str(st);
    } else if (!strcmp(func, "install_tool_callbacks")) {
        st = copy_obj_field(in, "self");
        r = cpy_install_tool_callbacks(&io, st);
        state = echo_keys(st, CB_KEYS, 1);
    } else if (!strcmp(func, "show_security_advisories")) {
        r = cpy_show_security_advisories(&io, NULL);
    } else if (!strcmp(func, "finalize_preloaded_skills")) {
        st = copy_obj_field(in, "self");
        r = cpy_finalize_preloaded_skills(&io, st);
        state = echo_keys(st, FIN_KEYS, 3);
    } else if (!strcmp(func, "slash_handler")) {
        r = cpy_slash_handler(J("dispatch"), J("handlers"), J("canonical"));
    } else if (!strcmp(func, "process_unregistered_slash")) {
        st = copy_obj_field(in, "self");
        r = cpy_process_unregistered_slash(&io, st, J("cmd_original"), J("cmd_lower"));
        state = echo_keys(st, PI_KEYS, 1);
    } else if (!strcmp(func, "run_quick_command")) {
        st = copy_obj_field(in, "self");
        r = cpy_run_quick_command(&io, st, J("base_cmd"), J("qcmd"), J("user_args"));
    } else if (!strcmp(func, "run_plugin_slash_command")) {
        st = copy_obj_field(in, "self");
        r = cpy_run_plugin_slash_command(&io, st, J("base_cmd"), J("user_args"));
    } else if (!strcmp(func, "queue_skill_message")) {
        st = copy_obj_field(in, "self");
        r = cpy_queue_skill_message(&io, st, J("msg"));
        state = echo_keys(st, PI_KEYS, 1);
    } else if (!strcmp(func, "run_skill_bundle_command")) {
        st = copy_obj_field(in, "self");
        r = cpy_run_skill_bundle_command(&io, st, J("base_cmd"), J("bundle_info"),
                                         J("user_instruction"));
        state = echo_keys(st, PI_KEYS, 1);
    } else if (!strcmp(func, "queue_loaded_skills")) {
        st = copy_obj_field(in, "self");
        r = cpy_queue_loaded_skills(&io, st, J("msg"), J("label"), J("missing"));
        state = echo_keys(st, PI_KEYS, 1);
    } else if (!strcmp(func, "run_skill_slash_command")) {
        st = copy_obj_field(in, "self");
        r = cpy_run_skill_slash_command(&io, st, J("base_cmd"), J("skill_info"), J("rest"));
        state = echo_keys(st, PI_KEYS, 1);
    } else if (!strcmp(func, "expand_slash_prefix")) {
        st = copy_obj_field(in, "self");
        r = cpy_expand_slash_prefix(&io, st, J("cmd_original"), J("cmd_lower"),
                                    J("skill_commands"), J("skill_bundles"));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
        r = make_raise("RuntimeError", "unknown func");
    }

    ret_json(out, r);
    json_set(out, "trace", ctx.trace ? ctx.trace : json_array());
    if (state) json_set(out, "state", state);
    json_free(st);
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