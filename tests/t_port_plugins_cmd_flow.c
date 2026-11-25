/*
 * t_port_plugins_cmd_flow.c — oracle harness for the unclaimed hermes_cli/plugins_cmd.py
 * FLOW cores + leaves ported in src/cli/plugins_cmd_flow.c.
 *
 * Self-contained: includes the port .c by AREA-QUALIFIED name. Reads JSON-line
 * cases from stdin (tests/fixtures/plugins_cmd_flow.jsonl), wires the universal seam
 * to the SCRIPTED results in each case's "seams" object (a recording stub appending
 * one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...], "state":{...}?}}.
 *
 * State-mutating ports echo an out.state window: discard_key_and_leaf -> {names},
 * apply_activation -> {enabled, disabled}, scan_level -> {seen} (only on a normal
 * return; a propagated raise drops the state window, matching the live drive).
 *
 * Fail-closed: a missing or empty seam script yields a RuntimeError raise spec
 * (recorded as the event ret, then propagated/swallowed by the port exactly as
 * live). A scripted JSON null is delivered as a NULL pointer (Python None).
 * tests/oracle_plugins_cmd_flow.py drives the LIVE Python with the same scripts and
 * compares the ordered decision sequence + return (+ state).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/plugins_cmd_flow.c"

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
            char *m = plc_fmt("no script for %s", name);
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
        char *m = plc_fmt("no script for %s", name);
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
    plc_io_t io = {script_call, &ctx, J("env")};
    json_t *r = NULL;
    json_t *state = NULL;

    if (!strcmp(func, "console")) {
        r = plc_console(&io);
    } else if (!strcmp(func, "table")) {
        r = plc_table(&io, J("columns"), J("kwargs"));
    } else if (!strcmp(func, "is_tty")) {
        r = plc_is_tty(&io);
    } else if (!strcmp(func, "fail")) {
        r = plc_fail(&io, J("console"), SD("message", ""));
    } else if (!strcmp(func, "ask_yes")) {
        r = plc_ask_yes(&io, SD("prompt", ""));
    } else if (!strcmp(func, "config_value")) {
        r = plc_config_value(&io, J("keys"), J("default"));
    } else if (!strcmp(func, "clone_timeout_seconds")) {
        r = plc_clone_timeout_seconds(&io);
    } else if (!strcmp(func, "config_name_set")) {
        r = plc_config_name_set(&io, J("keys"));
    } else if (!strcmp(func, "config_str")) {
        r = plc_config_str(&io, J("keys"), J("default"));
    } else if (!strcmp(func, "write_config_value")) {
        r = plc_write_config_value(&io, SD("section", ""), SD("key", ""), J("value"));
    } else if (!strcmp(func, "native_manifest_file")) {
        r = plc_native_manifest_file(&io, SD("plugin_dir", ""));
    } else if (!strcmp(func, "has_portable_manifest")) {
        r = plc_has_portable_manifest(&io, SD("plugin_dir", ""));
    } else if (!strcmp(func, "load_yaml_manifest")) {
        r = plc_load_yaml_manifest(&io, SD("manifest_file", ""));
    } else if (!strcmp(func, "looks_like_plugin_dir")) {
        r = plc_looks_like_plugin_dir(&io, SD("target", ""));
    } else if (!strcmp(func, "read_manifest_info")) {
        r = plc_read_manifest_info(&io, SD("d", ""), SD("prefix", ""));
    } else if (!strcmp(func, "missing_env_specs")) {
        r = plc_missing_env_specs(&io, J("manifest"));
    } else if (!strcmp(func, "clone_failure_message")) {
        r = plc_clone_failure_message(&io, SD("git_url", ""), SD("git_error", ""));
    } else if (!strcmp(func, "unknown_plugin_message")) {
        r = plc_unknown_plugin_message(&io, SD("name", ""), json_get_bool(in, "downloaded_only", false));
    } else if (!strcmp(func, "preserved_files_note")) {
        r = plc_preserved_files_note(J("exc"), J("merged"));
    } else if (!strcmp(func, "scan_merged_tree")) {
        r = plc_scan_merged_tree(&io, SD("plugin_dir", ""), SD("identifier", ""), J("merged"), J("kwargs"));
    } else if (!strcmp(func, "discard_key_and_leaf")) {
        r = plc_discard_key_and_leaf(J("names"), SD("key", ""));
        state = json_object();
        json_set(state, "names", r);
        r = NULL;
    } else if (!strcmp(func, "plugin_aliases")) {
        r = plc_plugin_aliases(&io, SD("key", ""), J("entries"));
    } else if (!strcmp(func, "disabled_by_update")) {
        r = plc_disabled_by_update(&io, SD("key", ""), J("aliases"));
    } else if (!strcmp(func, "activate_key")) {
        r = plc_activate_key(&io, SD("key", ""), json_get_bool(in, "enable", false), J("console"));
    } else if (!strcmp(func, "apply_activation")) {
        state = plc_apply_activation(J("enabled"), J("disabled"), SD("key", ""), J("aliases"),
                                     json_get_bool(in, "enable", false));
        r = NULL;
    } else if (!strcmp(func, "forget_plugin_config")) {
        r = plc_forget_plugin_config(&io, J("aliases"));
    } else if (!strcmp(func, "plugin_selection_version")) {
        r = plc_plugin_selection_version(&io);
    } else if (!strcmp(func, "refuse_recalled_entry")) {
        r = plc_refuse_recalled_entry(&io, J("entry"));
    } else if (!strcmp(func, "tri_state_flag")) {
        r = plc_tri_state_flag(J("args"), SD("yes_attr", ""), SD("no_attr", ""));
    } else if (!strcmp(func, "category_active_names")) {
        r = plc_category_active_names(&io);
    } else if (!strcmp(func, "saved_list_toolset_key")) {
        r = plc_saved_list_toolset_key(&io, SD("name", ""));
    } else if (!strcmp(func, "scan_level")) {
        json_t *seen = J("seen") ? json_copy(J("seen")) : json_object();
        if (seen->type != JSON_OBJECT) { json_free(seen); seen = json_object(); }
        r = plc_scan_level(&io, SD("base", ""), SD("source", ""), J("skip_names"), SD("prefix", ""),
                           (int)json_get_num(in, "depth", 0), seen);
        if (!plc_is_raise(r)) {
            state = json_object();
            json_set(state, "seen", seen);
            r = NULL;
        } else {
            json_free(seen);
        }
    } else if (!strcmp(func, "discover_all_plugins")) {
        r = plc_discover_all_plugins(&io);
    } else if (!strcmp(func, "default_on")) {
        r = plc_default_on(&io, SD("dir_path", ""), SD("source", ""));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
        r = make_raise("RuntimeError", "unknown func");
    }

    json_set(out, "ret", r ? r : json_null());
    json_set(out, "trace", ctx.trace ? ctx.trace : json_array());
    if (state) json_set(out, "state", state);
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