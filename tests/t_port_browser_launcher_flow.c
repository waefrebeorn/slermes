/*
 * t_port_browser_launcher_flow.c — oracle harness for the
 * tools/browser_launcher.py launch-decision FLOW cores ported in
 * src/tools/browser_launcher_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * One universal seam stub consumes the case's "io" scripts (absent/null ->
 * None; {"seq": [...]} -> per-call sequence, None when exhausted; any other
 * value -> reused), records one {ev, args, ret} event per call in call order
 * (ret = the RAW scripted value, raise-specs included), and emits
 * {"func": ..., "in": ..., "out": {"ret": ..., "raise": ..., "trace": [...],
 * "state": {"served_homes": [...]}}} per case.
 * tests/oracle_browser_launcher_flow.py drives the LIVE Python with the same
 * scripts and compares the ordered trace + ret + raise + state.
 *
 * Gate: bash tests/run_one_oracle.sh browser_launcher_flow \
 *           oracle_browser_launcher_flow.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "browser_launcher_flow.c"

typedef struct {
    const json_t *in;
    json_t *trace;    /* owned JSON array of events */
    json_t *counts;   /* owned object: seam name -> call count */
} blf_script_ctx_t;

static json_t *blf_script_next(blf_script_ctx_t *c, const char *name) {
    const json_t *io = json_obj_get(c->in, "io");
    const json_t *s = io ? json_obj_get(io, name) : NULL;
    if (!s || s->type == JSON_NULL) return NULL;
    if (s->type == JSON_OBJECT) {
        const json_t *seq = json_obj_get(s, "seq");
        if (seq) {
            double n = json_get_num(c->counts, name, 0);
            json_set(c->counts, name, json_number(n + 1));
            if (seq->type == JSON_ARRAY && (size_t)n < seq->c.count)
                return json_copy(seq->c.items[(size_t)n]);
            return NULL;  /* exhausted -> None */
        }
    }
    return json_copy(s);
}

static json_t *blf_stub_call(void *ctx, const char *name, const json_t *args) {
    blf_script_ctx_t *c = (blf_script_ctx_t *)ctx;
    json_t *r = blf_script_next(c, name);
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args ? json_copy(args) : json_object());
    json_set(e, "ret", r ? json_copy(r) : json_null());
    json_append(c->trace, e);
    return r;  /* ownership transfers to blf_call */
}

typedef json_t *(*blf_fn_t)(const json_t *, const blf_io_t *, json_t *, json_t **);
typedef struct { const char *func; blf_fn_t fn; } blf_dispatch_t;

static const blf_dispatch_t BLF_DISPATCH[] = {
    { "running_in_docker", blf_running_in_docker },
    { "apparmor_restricts_unprivileged_userns", blf_apparmor_restricts },
    { "needs_chromium_sandbox_bypass", blf_needs_sandbox_bypass },
    { "_explicit_executable_configured", blf_explicit_executable_configured },
    { "_resolve_executable", blf_resolve_executable },
    { "chromium_ready", blf_chromium_ready },
    { "_launch_timeout", blf_launch_timeout },
    { "_resolve_headed", blf_resolve_headed },
    { "_spawn_kwargs", blf_spawn_kwargs },
    { "_read_devtools_file", blf_read_devtools_file },
    { "_log_tail", blf_log_tail },
    { "_wait_for_devtools", blf_wait_for_devtools },
    { "_singleton_owner", blf_singleton_owner },
    { "_singleton_pid", blf_singleton_pid },
    { "running_instance", blf_running_instance },
    { "_default_user_data_dir", blf_default_user_data_dir },
    { "_prepare_dir", blf_prepare_dir },
    { "launch_chromium", blf_launch_chromium },
};
#define BLF_DISPATCH_N (sizeof(BLF_DISPATCH) / sizeof(BLF_DISPATCH[0]))

/* served_homes is a SET in the live module: sort before emitting so the
 * ordered-array comparison models set equality. */
static void blf_sort_homes(json_t *homes) {
    if (!homes || homes->type != JSON_ARRAY) return;
    for (size_t i = 1; i < homes->c.count; i++) {
        json_t *v = homes->c.items[i];
        size_t j = i;
        while (j > 0 && homes->c.items[j - 1]->type == JSON_STRING
               && v->type == JSON_STRING
               && strcmp(homes->c.items[j - 1]->str_val, v->str_val) > 0) {
            homes->c.items[j] = homes->c.items[j - 1];
            j--;
        }
        homes->c.items[j] = v;
    }
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    blf_script_ctx_t ctx = { in, json_array(), json_object() };
    blf_io_t io = { &ctx, blf_stub_call };
    json_t *state = json_object();
    json_set(state, "served_homes", json_array());
    json_t *exc = NULL;
    json_t *ret = NULL;

    blf_fn_t fn = NULL;
    for (size_t i = 0; i < BLF_DISPATCH_N; i++) {
        if (strcmp(BLF_DISPATCH[i].func, func) == 0) {
            fn = BLF_DISPATCH[i].fn;
            break;
        }
    }
    if (fn) {
        ret = fn(in, &io, state, &exc);
    } else {
        json_set(out, "error", json_string("unknown func"));
    }
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "raise", exc ? exc : json_null());
    json_set(out, "trace", ctx.trace);
    blf_sort_homes(json_obj_get(state, "served_homes"));
    json_set(out, "state", state);
    json_free(ctx.counts);
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    FILE *f = (argc > 1) ? fopen(argv[1], "rb") : stdin;
    if (!f) { fprintf(stderr, "cannot open fixture\n"); return 1; }
    char line[1 << 20];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '#') continue;
        char *err = NULL;
        json_t *c = json_parse(line, &err);
        if (err || !c || c->type != JSON_OBJECT) {
            fprintf(stderr, "bad case line: %s\n", err ? err : "not an object");
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
    if (f != stdin) fclose(f);
    return 0;
}
