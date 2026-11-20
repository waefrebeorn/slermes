/*
 * t_port_config_backend_flow2.c — oracle harness for the hermes_cli/config_backend.py
 * UNCLAIMED remainder ported in src/cli/config_backend_flow2.c.
 *
 * Self-contained: includes the port .c by area-qualified path. Reads JSON-line cases
 * from stdin (tests/fixtures/config_backend_flow2.jsonl), wires the universal seam to
 * the SCRIPTED results in each case's "seams" object (a recording stub appending one
 * trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 *
 * FAIL-CLOSED script semantics (identical to the oracle): a MISSING script returns a
 * RuntimeError raise spec {"__raise__":"RuntimeError","msg":"no script for seam: <name>"};
 * a scripted JSON null returns a NULL pointer (None), never a json_null node; an ARRAY
 * script is a per-call queue (last value repeats, empty array -> None);
 * {"__raise__": cls, "msg": ...} scripts an exception. The observable effects are the
 * ordered seam trace + the return value. tests/oracle_config_backend_flow2.py drives
 * the LIVE Python with the same scripts and compares ordered trace + ret.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/config_backend_flow2.c"

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
        char *m = cb2_xsf("no script for seam: %s", name);
        result = cb2_raise("RuntimeError", m);
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

static void ret_json(json_t *out, json_t *r) {
    json_set(out, "ret", r ? r : json_null());
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = {in, NULL, json_object()};
    cb2_seams_t seams = {&ctx, script_call};
    const cb2_seams_t *sp = &seams;
    json_t *r = NULL;

    /* ── FLOW cores ── */
    if (!strcmp(func, "refuse_redacted_sets")) {
        r = cb2_refuse_redacted_sets(sp, S("config_path"), J("sets"));
    } else if (!strcmp(func, "read_old")) {
        r = cb2_read_old(sp, S("config_path"));
    }
    /* ── exception constructors ── */
    else if (!strcmp(func, "config_write_error_init")) {
        r = cb2_config_write_error_init(S("message"), J("code"));
    } else if (!strcmp(func, "config_locked_error_init")) {
        r = cb2_config_locked_error_init(S("path"), S("locked_by"), J("message"));
    } else if (!strcmp(func, "config_value_error_init")) {
        r = cb2_config_value_error_init(S("message"), J("code"));
    }
    /* ── dataclass / instance constructors ── */
    else if (!strcmp(func, "explicit_file_init")) {
        r = cb2_explicit_file_init(S("path"));
    } else if (!strcmp(func, "user_layer_init")) {
        r = cb2_user_layer_init(J("fields"));
    } else if (!strcmp(func, "changes_init")) {
        r = cb2_changes_init(J("fields"));
    }
    /* ── ConfigBackend Protocol surface (all return None) ── */
    else if (!strcmp(func, "proto_read_user_doc")) {
        r = cb2_proto_read_user_doc();
    } else if (!strcmp(func, "proto_read_user_doc_readonly")) {
        r = cb2_proto_read_user_doc_readonly();
    } else if (!strcmp(func, "proto_version")) {
        r = cb2_proto_version();
    } else if (!strcmp(func, "proto_cache_version")) {
        r = cb2_proto_cache_version();
    } else if (!strcmp(func, "proto_exists")) {
        r = cb2_proto_exists();
    } else if (!strcmp(func, "proto_probe_readable")) {
        r = cb2_proto_probe_readable();
    } else if (!strcmp(func, "proto_write_changes")) {
        r = cb2_proto_write_changes();
    } else if (!strcmp(func, "proto_locked")) {
        r = cb2_proto_locked();
    } else if (!strcmp(func, "proto_supports_file_tooling")) {
        r = cb2_proto_supports_file_tooling();
    } else if (!strcmp(func, "proto_honors_managed_config")) {
        r = cb2_proto_honors_managed_config();
    } else if (!strcmp(func, "proto_boot")) {
        r = cb2_proto_boot();
    } else if (!strcmp(func, "proto_protected_env_names")) {
        r = cb2_proto_protected_env_names();
    } else if (!strcmp(func, "proto_deployment_env_names")) {
        r = cb2_proto_deployment_env_names();
    } else if (!strcmp(func, "proto_bootstrap_env_names")) {
        r = cb2_proto_bootstrap_env_names();
    } else if (!strcmp(func, "proto_preserve_read_base")) {
        r = cb2_proto_preserve_read_base();
    } else if (!strcmp(func, "proto_doctor_warnings")) {
        r = cb2_proto_doctor_warnings();
    } else if (!strcmp(func, "proto_apply_in_memory")) {
        r = cb2_proto_apply_in_memory();
    } else if (!strcmp(func, "proto_describe")) {
        r = cb2_proto_describe();
    } else if (!strcmp(func, "proto_destination")) {
        r = cb2_proto_destination();
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    ret_json(out, r);
    json_set(out, "trace", ctx.trace ? ctx.trace : json_array());
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
        /* Reserve envelope key positions: the oracle's want is built {ret, trace}
         * and canon() treats insertion order as semantic. */
        json_set(out, "ret", json_null());
        json_set(out, "trace", json_array());
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
