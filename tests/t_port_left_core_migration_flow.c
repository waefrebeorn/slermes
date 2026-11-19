/*
 * t_port_left_core_migration_flow.c — oracle harness for the
 * hermes_cli/left_core_migration.py FLOW cores + leaves ported in
 * src/cli/left_core_migration_flow.c.
 *
 * Self-contained: includes the port .c by AREA-QUALIFIED path. Reads JSON-line
 * cases from stdin (tests/fixtures/left_core_migration_flow.jsonl), wires the
 * universal seam to the SCRIPTED results in each case's "seams" object (a
 * recording stub appending one trace event per seam call, in call order), and
 * emits {"func":..., "in":..., "out":{"ret":..., "trace":[...]}} (plus "config"
 * for the in-place _mark mutation flow).
 *
 * Fail-closed: a missing or empty seam script yields a RuntimeError raise spec
 * (recorded as the event ret, then propagated/swallowed by the port exactly as
 * live). A scripted JSON null is delivered as a NULL pointer (None), never a
 * json_null node. {"__raise__": cls, "msg": ...} scripts an exception.
 * tests/oracle_left_core_migration_flow.py drives the LIVE Python with the same
 * scripts and compares the ordered decision sequence + return (+ mutated config).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/left_core_migration_flow.c"

typedef struct {
    const json_t *in;
    json_t *trace;
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
    if (result && result->type == JSON_NULL) { json_free(result); result = NULL; }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args_drop_null(args));   /* borrowed args: copy, do not free */
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

/* Borrow a JSON string array as a const char* vector (valid during the call). */
static const char **str_vec(const json_t *arr, size_t *n) {
    *n = 0;
    if (!is_arr(arr) || arr->c.count == 0) return NULL;
    const char **v = (const char **)malloc(arr->c.count * sizeof(char *));
    for (size_t i = 0; i < arr->c.count; i++) {
        const json_t *it = arr->c.items[i];
        v[i] = (is_str(it) && it->str_val) ? it->str_val : "";
    }
    *n = arr->c.count;
    return v;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = {in, NULL, json_object()};
    lcm_seams_t seams = {&ctx, script_call};
    const lcm_seams_t *sp = &seams;
    json_t *r = NULL;
    bool echo_config = false;
    json_t *config_copy = NULL;

    if (!strcmp(func, "_no_notice")) {
        r = lcm_no_notice(sp, SD("home", ""), B("process_env", false));
    } else if (!strcmp(func, "profile_env_names")) {
        r = lcm_profile_env_names(sp);
    } else if (!strcmp(func, "launch_env_names")) {
        r = lcm_launch_env_names(sp);
    } else if (!strcmp(func, "legacy_plugin_alias")) {
        r = lcm_legacy_plugin_alias(sp, SD("name", ""));
    } else if (!strcmp(func, "legacy_plugin_keys")) {
        r = lcm_legacy_plugin_keys(sp, SD("plugin", ""));
    } else if (!strcmp(func, "_provider_feature")) {
        r = lcm_provider_feature(sp, J("name"));
    } else if (!strcmp(func, "_install_sentence")) {
        r = lcm_install_sentence(sp, SD("feature", ""));
    } else if (!strcmp(func, "platform_install_hint")) {
        r = lcm_platform_install_hint(sp, SD("platform", ""));
    } else if (!strcmp(func, "terminal_backend_install_hint")) {
        r = lcm_terminal_backend_install_hint(sp, SD("backend", ""));
    } else if (!strcmp(func, "moved_plugin_hint")) {
        r = lcm_moved_plugin_hint(sp, SD("name", ""));
    } else if (!strcmp(func, "_load_error")) {
        r = lcm_load_error(sp, SD("feature", ""));
    } else if (!strcmp(func, "provider_left_core")) {
        r = lcm_provider_left_core(sp, J("name"));
    } else if (!strcmp(func, "provider_install_hint")) {
        r = lcm_provider_install_hint(sp, J("name"));
    } else if (!strcmp(func, "feature_in_use")) {
        r = lcm_feature_in_use(sp, SD("feature", ""), SD("home", ""), B("process_env", false));
    } else if (!strcmp(func, "host_install_hint")) {
        r = lcm_host_install_hint(sp, J("base_url"));
    } else if (!strcmp(func, "moved_command_hint")) {
        r = lcm_moved_command_hint(sp, SD("prog", ""), SD("value", ""), SD("action", ""));
    } else if (!strcmp(func, "_noticed")) {
        r = lcm_noticed(sp, SD("message", ""), SD("home", ""), SD("feature", ""),
                        B("process_env", false));
    } else if (!strcmp(func, "_read_config")) {
        r = lcm_read_config(sp, SD("home", ""));
    } else if (!strcmp(func, "_auth_store")) {
        r = lcm_auth_store(sp, SD("home", ""));
    } else if (!strcmp(func, "plugin_present")) {
        r = lcm_plugin_present(sp, SD("plugin", ""), SD("home", ""));
    } else if (!strcmp(func, "agent_imports_in_use")) {
        r = lcm_agent_imports_in_use(sp, SD("home", ""), B("process_env", false));
    } else if (!strcmp(func, "_plugins_listed")) {
        size_t n; const char **names = str_vec(J("names"), &n);
        r = lcm_plugins_listed(sp, SD("home", ""), names, n);
        free(names);
    } else if (!strcmp(func, "_plugin_lists")) {
        size_t n; const char **also = str_vec(J("also"), &n);
        r = lcm_plugin_lists(sp, SD("home", ""), SD("plugin", ""), also, n);
        free(also);
    } else if (!strcmp(func, "_bundled_plugin_selected")) {
        r = lcm_bundled_plugin_selected(sp, SD("home", ""), SD("plugin", ""));
    } else if (!strcmp(func, "langfuse_in_use")) {
        r = lcm_langfuse_in_use(sp, SD("home", ""), B("process_env", false));
    } else if (!strcmp(func, "spotify_in_use")) {
        r = lcm_spotify_in_use(sp, SD("home", ""), B("process_env", false));
    } else if (!strcmp(func, "_home_env")) {
        r = lcm_home_env(sp, SD("home", ""), SD("name", ""), B("process_env", false));
    } else if (!strcmp(func, "camofox_in_use")) {
        r = lcm_camofox_in_use(sp, SD("home", ""), B("process_env", false));
    } else if (!strcmp(func, "singularity_in_use")) {
        r = lcm_singularity_in_use(sp, SD("home", ""), B("process_env", false));
    } else if (!strcmp(func, "homeassistant_in_use")) {
        r = lcm_homeassistant_in_use(sp, SD("home", ""), B("process_env", false));
    } else if (!strcmp(func, "platform_configured_on")) {
        r = lcm_platform_configured_on(sp, SD("home", ""), SD("name", ""));
    } else if (!strcmp(func, "_platform_config")) {
        r = lcm_platform_config(sp, J("block"));
    } else if (!strcmp(func, "_yuanbao_notice")) {
        r = lcm_yuanbao_notice(sp, SD("home", ""), B("process_env", false));
    } else if (!strcmp(func, "_notice_file")) {
        r = lcm_notice_file(SD("home", ""));
    } else if (!strcmp(func, "_scope_kept")) {
        r = lcm_scope_kept(sp, J("config"), SD("feature", ""));
    } else if (!strcmp(func, "_mark")) {
        config_copy = is_obj(J("config")) ? json_copy(J("config")) : json_object();
        r = lcm_mark(config_copy, SD("key", ""), SD("feature", ""));
        echo_config = true;
    } else if (!strcmp(func, "defer_startup_notice")) {
        r = lcm_defer_startup_notice(sp, SD("message", ""), B("log", true));
    } else if (!strcmp(func, "_say_post_split")) {
        r = lcm_say_post_split(sp, SD("home", ""), SD("feature", ""),
                               B("backoff", false), B("process_env", false));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    ret_json(out, r);
    json_set(out, "trace", ctx.trace ? ctx.trace : json_array());
    if (echo_config) json_set(out, "config", config_copy ? config_copy : json_null());
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
         * (and config for _mark); canon() treats insertion order as semantic. */
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