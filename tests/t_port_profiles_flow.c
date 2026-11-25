/*
 * t_port_profiles_flow.c — oracle harness for the unclaimed hermes_cli/profiles.py
 * FLOW cores + leaves ported in src/cli/profiles_flow.c.
 *
 * Self-contained: includes the port .c by AREA-QUALIFIED name. Reads JSON-line
 * cases from stdin (tests/fixtures/profiles_flow.jsonl), wires the universal seam
 * to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 *
 * None of the ported functions mutate caller-held input state (the only mutated
 * dict, _strip_multiplex_flag's cfg, is seam-produced and captured in the
 * atomic_config_replace trace args), so no out.state window is emitted.
 *
 * Fail-closed: a missing or empty seam script yields a RuntimeError raise spec
 * (recorded as the event ret, then propagated/swallowed by the port exactly as
 * live). A scripted JSON null is delivered as a NULL pointer (Python None).
 * tests/oracle_profiles_flow.py drives the LIVE Python with the same scripts and
 * compares the ordered decision sequence + return.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/profiles_flow.c"

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
            char *m = pf_fmt("no script for %s", name);
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
        char *m = pf_fmt("no script for %s", name);
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

#define SD(k, d) json_get_str(in, k, d)
#define J(k) json_obj_get(in, k)

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = {in, NULL, json_object()};
    pf_io_t io = {script_call, &ctx, J("env")};
    json_t *r = NULL;

    if (!strcmp(func, "fold")) {
        r = pf_fold(J("parts"));
    } else if (!strcmp(func, "path_is_private")) {
        r = pf_profile_path_is_private(J("parts"));
    } else if (!strcmp(func, "path_contains_private_store")) {
        r = pf_profile_path_contains_private_store(J("parts"));
    } else if (!strcmp(func, "suggest_profile_name")) {
        r = pf_suggest_profile_name(SD("name", ""));
    } else if (!strcmp(func, "invalid_profile_name_error")) {
        r = pf_invalid_profile_name_error(SD("name", ""));
    } else if (!strcmp(func, "missing_profile_error")) {
        r = pf_missing_profile_error(SD("canon", ""));
    } else if (!strcmp(func, "unknown_profile_error")) {
        r = pf_unknown_profile_error(SD("canon", ""));
    } else if (!strcmp(func, "profile_exists_error")) {
        r = pf_profile_exists_error(SD("canon", ""));
    } else if (!strcmp(func, "clean_previous_names")) {
        r = pf_clean_previous_names(J("raw"));
    } else if (!strcmp(func, "is_hermes_argv")) {
        r = pf_is_hermes_argv(J("argv"));
    } else if (!strcmp(func, "argv_profile_selectors")) {
        r = pf_argv_profile_selectors(J("argv"));
    } else if (!strcmp(func, "profile_root_for_env_home")) {
        r = pf_profile_root_for_env_home(SD("env_home", ""), SD("default_root", ""));
    } else if (!strcmp(func, "parked_marker_path")) {
        r = pf_parked_marker_path(SD("home", ""));
    } else if (!strcmp(func, "export_credential_entries")) {
        r = pf_export_credential_entries(SD("directory", ""), J("contents"));
    } else if (!strcmp(func, "standalone_truthy")) {
        r = pf_standalone_truthy(&io, J("value"));
    } else if (!strcmp(func, "canon_valid")) {
        r = pf_canon_valid(&io, SD("name", ""));
    } else if (!strcmp(func, "existing_profile_dir")) {
        r = pf_existing_profile_dir(&io, SD("name", ""));
    } else if (!strcmp(func, "wrapper_path")) {
        r = pf_wrapper_path(&io, SD("alias", ""));
    } else if (!strcmp(func, "is_our_wrapper")) {
        r = pf_is_our_wrapper(&io, SD("path", ""));
    } else if (!strcmp(func, "find_our_wrapper")) {
        r = pf_find_our_wrapper(&io, SD("alias", ""));
    } else if (!strcmp(func, "non_exportable_entries")) {
        r = pf_non_exportable_entries(&io, SD("directory", ""), J("contents"));
    } else if (!strcmp(func, "notify_multiplexer")) {
        r = pf_notify_multiplexer(&io, SD("canon", ""));
    } else if (!strcmp(func, "purge_identity")) {
        r = pf_purge_identity(&io, SD("canon", ""));
    } else if (!strcmp(func, "live_default_multiplexer")) {
        r = pf_live_default_multiplexer(&io);
    } else if (!strcmp(func, "served_by_running_multiplexer")) {
        r = pf_served_by_running_multiplexer(&io, SD("profile_name", ""));
    } else if (!strcmp(func, "retarget_active_profile")) {
        r = pf_retarget_active_profile(&io, SD("old", ""), SD("new", ""), SD("message", ""));
    } else if (!strcmp(func, "current_profile_name")) {
        r = pf_current_profile_name(&io, J("default"), J("environ"));
    } else if (!strcmp(func, "inside_git_checkout")) {
        r = pf_inside_git_checkout(&io, SD("path", ""));
    } else if (!strcmp(func, "s6_runtime_manager")) {
        r = pf_s6_runtime_manager(&io);
    } else if (!strcmp(func, "refuse_rename_without_file_tooling")) {
        r = pf_refuse_rename_without_file_tooling(&io, SD("old_canon", ""));
    } else if (!strcmp(func, "load_yaml_dict")) {
        r = pf_load_yaml_dict(&io, SD("path", ""));
    } else if (!strcmp(func, "seed_file_if_missing")) {
        r = pf_seed_file_if_missing(&io, SD("path", ""), SD("text", ""), J("mode"));
    } else if (!strcmp(func, "clone_file")) {
        r = pf_clone_file(&io, SD("source_dir", ""), SD("profile_dir", ""), SD("relpath", ""));
    } else if (!strcmp(func, "resolve_clone_source")) {
        r = pf_resolve_clone_source(&io, J("clone_from"));
    } else if (!strcmp(func, "seed_model_config")) {
        r = pf_seed_model_config(&io, SD("profile_dir", ""));
    } else if (!strcmp(func, "strip_multiplex_flag")) {
        r = pf_strip_multiplex_flag(&io, SD("config_path", ""));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
        r = make_raise("RuntimeError", "unknown func");
    }

    json_set(out, "ret", r ? r : json_null());
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
