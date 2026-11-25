/*
 * t_port_commands_flow.c — oracle harness for the command-surface FLOW cores
 * + leaves ported in src/cli/commands_flow.c (hermes_cli/commands.py +
 * commands_platforms.py / commands_completion.py / slash_exec.py /
 * cli_unknown_command.py).
 *
 * Self-contained: includes the port .c by AREA-QUALIFIED name. Reads JSON-line
 * cases from stdin (tests/fixtures/commands_flow.jsonl), wires the universal
 * seam to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 *
 * Fail-closed: a missing or empty seam script yields a RuntimeError raise spec
 * (recorded as the event ret, then propagated/swallowed by the port exactly as
 * live). An uncaught raise parks in cf_pending and is emitted as ret.
 * tests/oracle_commands_flow.py drives the LIVE Python with the same scripts
 * and compares the ordered decision sequence + return.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/commands_flow.c"

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
    cf_seams_t seams = {&ctx, script_call};
    const cf_seams_t *sp = &seams;
    cf_take_pending();  /* clear any stale parked raise */

    if (!strcmp(func, "localized")) {
        ret_json(out, cf_localized(SD("key", ""), SD("default", ""), sp));
    } else if (!strcmp(func, "category_label")) {
        ret_json(out, cf_category_label(S("category"), sp));
    } else if (!strcmp(func, "describe")) {
        ret_json(out, cf_describe(J("cmd"), sp));
    } else if (!strcmp(func, "desktop_surface_registry")) {
        ret_json(out, cf_desktop_surface_registry(J("registry")));
    } else if (!strcmp(func, "build_commands_by_category")) {
        ret_json(out, cf_build_commands_by_category(J("registry"), sp));
    } else if (!strcmp(func, "build_commands")) {
        ret_json(out, cf_build_commands(J("registry"), sp));
    } else if (!strcmp(func, "is_gateway_available")) {
        ret_bool(out, cf_is_gateway_available(J("cmd"), J("overrides"), sp));
    } else if (!strcmp(func, "module_getattr")) {
        ret_json(out, cf_module_getattr(S("name"), sp));
    } else if (!strcmp(func, "gateway_available_commands")) {
        ret_json(out, cf_gateway_available_commands(J("registry"), sp));
    } else if (!strcmp(func, "normalize_telegram_desc")) {
        ret_str(out, cf_normalize_telegram_desc(SD("desc", "")));
    } else if (!strcmp(func, "truncate_desc")) {
        ret_str(out, cf_truncate_desc(SD("desc", ""), json_get_num(in, "limit", 100)));
    } else if (!strcmp(func, "sanitized_rank")) {
        ret_json(out, cf_sanitized_rank(J("raw_names"), sp));
    } else if (!strcmp(func, "iter_gateway_skills")) {
        ret_json(out, cf_iter_gateway_skills(in, sp));
    } else if (!strcmp(func, "gateway_bundles")) {
        ret_json(out, cf_gateway_bundles(sp));
    } else if (!strcmp(func, "completion")) {
        ret_json(out, cf_completion(SD("text", ""), SD("word", ""), SD("display", ""), J("meta")));
    } else if (!strcmp(func, "prefix_completions")) {
        ret_json(out, cf_prefix_completions(J("rows"), SD("partial", ""), B("skip_exact", true)));
    } else if (!strcmp(func, "split_args")) {
        ret_json(out, cf_split_args(SD("sub_text", "")));
    } else if (!strcmp(func, "dir_completions")) {
        ret_json(out, cf_dir_completions(in, sp));
    } else if (!strcmp(func, "call_provider")) {
        ret_json(out, cf_call_provider(B("provider_present", false), sp));
    } else if (!strcmp(func, "exec_autoprune")) {
        ret_json(out, cf_exec_autoprune(J("ctx"), in, sp));
    } else if (!strcmp(func, "skill_commands")) {
        ret_json(out, cf_skill_commands(in, sp));
    } else if (!strcmp(func, "unknown_command_lines")) {
        ret_json(out, cf_unknown_command_lines(SD("typed", ""), J("known"), sp));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
        ret_json(out, NULL);
    }

    /* An uncaught raise overrides the normal return. */
    json_t *pending = cf_take_pending();
    if (pending) json_set(out, "ret", pending);

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
