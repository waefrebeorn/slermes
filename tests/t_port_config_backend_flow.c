/*
 * t_port_config_backend_flow.c — oracle harness for the hermes_cli/config_backend.py
 * FLOW cores + leaves ported in src/cli/config_backend_flow.c.
 *
 * Self-contained: includes the port .c by area-qualified path. Reads JSON-line
 * cases from stdin (tests/fixtures/config_backend_flow.jsonl), wires the universal
 * seam to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 * A scripted JSON null returns a NULL pointer (None), never a json_null node;
 * {"__raise__": cls, "msg": ...} scripts an exception. The flows are stateless
 * apart from the contextvar managers (working copy of "cv"); the observable
 * effects are the ordered seam trace + the return value.
 * tests/oracle_config_backend_flow.py drives the LIVE Python with the same scripts
 * and compares ordered trace + ret.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/config_backend_flow.c"

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
    if (sc && sc->type == JSON_ARRAY) {
        double idx = json_get_num(ctx->counters, name, 0);
        size_t i = (size_t)idx;
        size_t n = json_len(sc);
        if (n > 0) result = json_copy(json_get(sc, i < n ? i : n - 1));
        json_set(ctx->counters, name, json_number(idx + 1));
    } else if (sc) {
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
    cb_seams_t seams = {&ctx, script_call};
    const cb_seams_t *sp = &seams;
    json_t *r = NULL;

    /* ── module-level FLOW cores ── */
    if (!strcmp(func, "drop_other_access")) {
        r = cb_drop_other_access(sp, S("path"));
    } else if (!strcmp(func, "bootstrap_names")) {
        r = cb_bootstrap_names(sp, S("kind"));
    } else if (!strcmp(func, "bootstrap_deployment")) {
        r = cb_bootstrap_deployment(sp, S("home"), S("project_env"));
    } else if (!strcmp(func, "local_file_layer")) {
        r = cb_local_file_layer(S("config_path"));
    } else if (!strcmp(func, "read_config_doc_readonly")) {
        r = cb_read_config_doc_readonly(sp, S("config_path"));
    } else if (!strcmp(func, "config_version")) {
        r = cb_config_version(sp, S("config_path"));
    } else if (!strcmp(func, "config_cache_version")) {
        r = cb_config_cache_version(sp, S("config_path"));
    } else if (!strcmp(func, "config_version_or_none")) {
        r = cb_config_version_or_none(sp, S("config_path"));
    } else if (!strcmp(func, "probe_config_readable")) {
        r = cb_probe_config_readable(sp, S("config_path"));
    } else if (!strcmp(func, "config_exists")) {
        r = cb_config_exists(sp, S("config_path"));
    } else if (!strcmp(func, "write_config_document")) {
        r = cb_write_config_document(sp, S("config_path"), J("document"), J("extra_content_on_create"));
    } else if (!strcmp(func, "config_destination")) {
        r = cb_config_destination(sp, S("config_path"));
    } else if (!strcmp(func, "write_config_key")) {
        r = cb_write_config_key(sp, S("config_path"), S("key_path"), J("value"));
    } else if (!strcmp(func, "keeping_read_base")) {
        r = cb_keeping_read_base(sp, S("config_path"));
    } else if (!strcmp(func, "write_changes_flow")) {
        r = cb_write_changes_flow(sp, S("config_path"), J("changes"));
    } else if (!strcmp(func, "deployment_env_names_flow")) {
        r = cb_deployment_env_names_flow(sp);
    } else if (!strcmp(func, "supports_file_tooling_flow")) {
        r = cb_supports_file_tooling_flow(sp);
    } else if (!strcmp(func, "require_file_tooling")) {
        r = cb_require_file_tooling(sp, S("what"));
    } else if (!strcmp(func, "refusing_locked_omissions")) {
        json_t *cv = json_copy(J("cv") ? J("cv") : json_object());
        r = cb_refusing_locked_omissions(sp, J("prefixes"), cv);
        json_free(cv);
    } else if (!strcmp(func, "pinning_keys")) {
        json_t *cv = json_copy(J("cv") ? J("cv") : json_object());
        r = cb_pinning_keys(sp, J("keys"), cv);
        json_free(cv);
    } else if (!strcmp(func, "refused_locked_omission_prefixes")) {
        r = cb_refused_locked_omission_prefixes(J("cv"));
    } else if (!strcmp(func, "pinned_keys")) {
        r = cb_pinned_keys(J("cv"));
    }
    /* ── FileBackend leaves ── */
    else if (!strcmp(func, "file_config_path")) {
        r = cb_file_config_path(S("home"));
    } else if (!strcmp(func, "file_version_path")) {
        r = cb_file_version_path(sp, S("path"));
    } else if (!strcmp(func, "file_read_path")) {
        r = cb_file_read_path(sp, S("path"));
    } else if (!strcmp(func, "file_read_path_readonly")) {
        r = cb_file_read_path_readonly(sp, S("path"));
    } else if (!strcmp(func, "file_read_user_doc")) {
        r = cb_file_read_user_doc(sp, S("home"));
    } else if (!strcmp(func, "file_read_user_doc_readonly")) {
        r = cb_file_read_user_doc_readonly(sp, S("home"));
    } else if (!strcmp(func, "file_version")) {
        r = cb_file_version(sp, S("home"));
    } else if (!strcmp(func, "file_cache_version")) {
        r = cb_file_cache_version(sp, S("home"), J("version"));
    } else if (!strcmp(func, "file_exists")) {
        r = cb_file_exists(sp, S("home"));
    } else if (!strcmp(func, "file_probe_readable")) {
        r = cb_file_probe_readable(sp, S("home"));
    } else if (!strcmp(func, "file_write_changes")) {
        r = cb_file_write_changes(sp, S("home"), J("changes"));
    } else if (!strcmp(func, "file_locked")) {
        r = cb_file_locked(S("home"), S("dotted"));
    } else if (!strcmp(func, "file_supports_file_tooling")) {
        r = cb_file_supports_file_tooling();
    } else if (!strcmp(func, "file_honors_managed_config")) {
        r = cb_file_honors_managed_config();
    } else if (!strcmp(func, "file_boot")) {
        r = cb_file_boot(S("home"));
    } else if (!strcmp(func, "file_protected_env_names")) {
        r = cb_file_protected_env_names();
    } else if (!strcmp(func, "file_deployment_env_names")) {
        r = cb_file_deployment_env_names();
    } else if (!strcmp(func, "file_bootstrap_env_names")) {
        r = cb_file_bootstrap_env_names();
    } else if (!strcmp(func, "file_preserve_read_base")) {
        r = cb_file_preserve_read_base();
    } else if (!strcmp(func, "file_doctor_warnings")) {
        r = cb_file_doctor_warnings(S("home"));
    } else if (!strcmp(func, "file_apply_in_memory")) {
        r = cb_file_apply_in_memory(S("home"), J("doc"));
    } else if (!strcmp(func, "file_describe")) {
        r = cb_file_describe(S("home"));
    } else if (!strcmp(func, "file_destination")) {
        r = cb_file_destination(S("home"));
    } else if (!strcmp(func, "explicit_config_path")) {
        r = cb_explicit_config_path(S("path"), S("home"));
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