/*
 * t_port_config_defaults_flow.c — oracle harness for the config-defaults FLOW port
 * (src/cli/config_defaults_flow.c; real defining modules hermes_cli/config_defaults_env.py
 * and hermes_cli/config_defaults_auxiliary.py — config_defaults.py is a pure-data leaf).
 *
 * Self-contained: includes the port .c by area-qualified path. Reads JSON-line cases
 * from stdin (tests/fixtures/config_defaults_flow.jsonl), wires the universal seam to
 * the SCRIPTED results in each case's "seams" object (a recording stub appending one
 * trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...], "state":...}}.
 *
 * FAIL-CLOSED script semantics (identical to the oracle): a MISSING script returns a
 * RuntimeError raise spec {"__raise__":"RuntimeError","msg":"no script for seam: <name>"};
 * a scripted JSON null returns a NULL pointer (None), never a json_null node; an ARRAY
 * script is a per-call queue (last value repeats, empty array -> None);
 * {"__raise__": cls, "msg": ...} scripts an exception. The observable effects are the
 * ordered seam trace + the return value + the post-call OptionalEnvVars state projection.
 * tests/oracle_config_defaults_flow.py drives the LIVE Python with the same scripts and
 * compares ordered trace + ret + state.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/config_defaults_flow.c"

typedef struct {
    const json_t *in;
    json_t *trace;
    json_t *counters;
} script_ctx_t;

static json_t *args_drop_null(const json_t *args) {
    json_t *out = json_object();
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++) {
            const json_t *v = args->c.items[i];
            if (!v || v->type == JSON_NULL) continue;
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
    if (!sc) {
        /* FAIL-CLOSED: a missing script is a RuntimeError raise spec (NOT None). */
        char *m = cd2_xsf("no script for seam: %s", name);
        result = cd2_raise("RuntimeError", m);
        free(m);
    } else if (sc->type == JSON_ARRAY) {
        double idx = json_get_num(ctx->counters, name, 0);
        size_t i = (size_t)idx;
        size_t n = json_len(sc);
        if (n > 0) result = json_copy(json_get(sc, i < n ? i : n - 1));
        json_set(ctx->counters, name, json_number(idx + 1));
    } else {
        result = json_copy(sc);
    }
    /* Contract: NULL == None. A scripted null must NOT surface as a non-NULL
     * json_null node (truthiness / is-not-None guards would read it wrong). */
    if (result && result->type == JSON_NULL) {
        json_free(result);
        result = NULL;
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args_drop_null(args));   /* borrowed args: copy, do not free */
    json_set(e, "ret", result ? json_copy(result) : json_null());
    if (!ctx->trace) ctx->trace = json_array();
    json_append(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define J(k) json_obj_get(in, k)

/* Build the OptionalEnvVars state projection {data, fill, published, filling_active}.
 * Takes ownership of st->data (moves it into the projection). */
static json_t *state_proj(cd2_env_state_t *st) {
    json_t *o = json_object();
    json_set(o, "data", st->data ? st->data : json_object());
    json_set(o, "fill", st->fill ? json_string(st->fill) : json_null());
    json_set(o, "published", st->published ? json_string(st->published) : json_null());
    json_set(o, "filling_active", json_bool(st->filling_active));
    st->data = NULL;  /* ownership moved */
    return o;
}

static void init_state(const json_t *in, cd2_env_state_t *st) {
    const json_t *s = json_obj_get(in, "state");
    const json_t *data = json_obj_get(s, "data");
    st->data = (data && data->type == JSON_OBJECT) ? json_copy(data) : json_object();
    st->fill = json_get_str(s, "fill", NULL);
    st->published = json_get_str(s, "published", NULL);
    st->filling_active = json_get_bool(s, "filling_active", false);
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = {in, NULL, json_object()};
    cd2_seams_t seams = {&ctx, script_call};
    const cd2_seams_t *sp = &seams;
    json_t *r = NULL;
    json_t *state = NULL;
    cd2_env_state_t st = {NULL, NULL, NULL, false};
    bool is_dict_method = false;

    /* ── pure entry factories (no instance state) ── */
    if (!strcmp(func, "aux")) {
        r = cd2_aux(J("timeout"), J("reasoning_effort"), J("extra"));
    } else if (!strcmp(func, "env")) {
        r = cd2_env(J("description"), J("prompt"), J("keys"));
    } else if (!strcmp(func, "make")) {
        r = cd2_make(J("bound"), J("call"));
    } else if (!strcmp(func, "base_url")) {
        r = cd2_base_url(sp, S("name"), S("prompt_name"));
    }
    /* ── OptionalEnvVars methods (mutate / read instance state) ── */
    else if (!strcmp(func, "arm")) {
        is_dict_method = true; init_state(in, &st);
        r = cd2_arm(&st, S("fill"), S("published"));
    } else if (!strcmp(func, "ready")) {
        is_dict_method = true; init_state(in, &st);
        r = cd2_ready(sp, &st);
    } else if (!strcmp(func, "getitem")) {
        is_dict_method = true; init_state(in, &st);
        r = cd2_getitem(sp, &st, S("key"));
    } else if (!strcmp(func, "get")) {
        is_dict_method = true; init_state(in, &st);
        r = cd2_get(sp, &st, S("key"), J("default"));
    } else if (!strcmp(func, "contains")) {
        is_dict_method = true; init_state(in, &st);
        r = cd2_contains(sp, &st, S("key"));
    } else if (!strcmp(func, "iter")) {
        is_dict_method = true; init_state(in, &st);
        r = cd2_iter(sp, &st);
    } else if (!strcmp(func, "len")) {
        is_dict_method = true; init_state(in, &st);
        r = cd2_len(sp, &st);
    } else if (!strcmp(func, "keys")) {
        is_dict_method = true; init_state(in, &st);
        r = cd2_keys(sp, &st);
    } else if (!strcmp(func, "items")) {
        is_dict_method = true; init_state(in, &st);
        r = cd2_items(sp, &st);
    } else if (!strcmp(func, "values")) {
        is_dict_method = true; init_state(in, &st);
        r = cd2_values(sp, &st);
    } else if (!strcmp(func, "copy")) {
        is_dict_method = true; init_state(in, &st);
        r = cd2_copy(sp, &st);
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    if (is_dict_method) state = state_proj(&st);
    json_set(out, "ret", r ? r : json_null());
    json_set(out, "trace", ctx.trace ? ctx.trace : json_array());
    json_set(out, "state", state ? state : json_null());
    json_free(ctx.counters);
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
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
        /* Reserve envelope key positions: the oracle's want is built {ret, trace,
         * state} and canon() treats insertion order as semantic. */
        json_set(out, "ret", json_null());
        json_set(out, "trace", json_array());
        json_set(out, "state", json_null());
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