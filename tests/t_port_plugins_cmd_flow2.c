/*
 * t_port_plugins_cmd_flow2.c — oracle harness for the UNCLAIMED remainder of
 * hermes_cli/plugins_cmd.py ported in src/cli/plugins_cmd_flow2.c (wave 2).
 *
 * Self-contained: includes the port .c by AREA-QUALIFIED name. Reads JSON-line
 * cases from stdin (tests/fixtures/plugins_cmd_flow2.jsonl), wires the universal
 * seam to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...], "state":{...}?}}.
 *
 * State-mutating ports echo an out.state window EVEN ON A RAISE (partial mutation
 * history): selection_lock -> {locks}; set_plugin_enabled -> {locks, enabled,
 * disabled}. The apply_activation seam's scripted ret may carry a "state" effect
 * the port applies to its working allow/deny sets (mirrored by the oracle stub).
 *
 * Fail-closed: a missing or empty seam script yields a RuntimeError raise spec
 * (recorded as the event ret, then propagated/swallowed by the port exactly as
 * live). A scripted JSON null is delivered as a NULL pointer (Python None).
 * tests/oracle_plugins_cmd_flow2.py drives the LIVE Python with the same scripts
 * and compares the ordered decision sequence + return (+ state).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/plugins_cmd_flow2.c"

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
            char *m = pc2_fmt("no script for %s", name);
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
        char *m = pc2_fmt("no script for %s", name);
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

static json_t *obj_or_empty(const json_t *v) {
    json_t *o = v ? json_copy(v) : json_object();
    if (o->type != JSON_OBJECT) { json_free(o); o = json_object(); }
    return o;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = {in, NULL, json_object()};
    pc2_io_t io = {script_call, &ctx, J("env")};
    json_t *r = NULL;
    json_t *state = NULL;

    if (!strcmp(func, "admit_and_save_plugin_sets")) {
        r = pc2_admit_and_save_plugin_sets(&io, J("enabled"), J("disabled"), J("extra_dirs"),
                                           J("console"), SD("action", "enable"),
                                           J("expected_config"), J("plugin"), J("settled"));
    } else if (!strcmp(func, "selection_lock")) {
        json_t *locks = obj_or_empty(J("locks"));
        r = pc2_selection_lock(&io, locks);
        state = json_object();
        json_set(state, "locks", locks);
    } else if (!strcmp(func, "set_plugin_enabled")) {
        json_t *locks = obj_or_empty(J("locks"));
        json_t *work = json_object();
        json_set(work, "enabled", json_array());
        json_set(work, "disabled", json_array());
        r = pc2_set_plugin_enabled(&io, SD("name", ""), json_get_bool(in, "enable", false),
                                   J("aliases"), J("console"), locks, work);
        state = json_object();
        json_set(state, "locks", locks);
        json_set(state, "enabled", json_copy(json_obj_get(work, "enabled")));
        json_set(state, "disabled", json_copy(json_obj_get(work, "disabled")));
        json_free(work);
    } else if (!strcmp(func, "catalog_installed_dir")) {
        r = pc2_catalog_installed_dir(&io, SD("name", ""));
    } else if (!strcmp(func, "find_plugin_entry")) {
        r = pc2_find_plugin_entry(&io, SD("name", ""));
    } else if (!strcmp(func, "enable_consent")) {
        r = pc2_enable_consent(&io, J("console"), SD("key", ""), J("allow_tool_override"));
    } else if (!strcmp(func, "resolved_user_plugin_dir")) {
        r = pc2_resolved_user_plugin_dir(&io, SD("name", ""));
    } else if (!strcmp(func, "catalog")) {
        r = pc2_catalog(&io);
    } else if (!strcmp(func, "action_update")) {
        r = pc2_action_update(&io, J("args"));
    } else if (!strcmp(func, "action_auto_update")) {
        r = pc2_action_auto_update(&io, J("args"));
    } else if (!strcmp(func, "action_pack")) {
        r = pc2_action_pack(&io, J("args"));
    } else if (!strcmp(func, "action_idle_tasks")) {
        r = pc2_action_idle_tasks(&io, J("args"));
    } else if (!strcmp(func, "refuse_legacy_relay")) {
        r = pc2_refuse_legacy_relay(&io, J("console"), SD("plugin", ""), J("legacy_keys"), SD("env", ""));
    } else if (!strcmp(func, "first_toolset")) {
        r = pc2_first_toolset(&io, J("tool_names"));
    } else if (!strcmp(func, "from_loaded_plugin")) {
        r = pc2_from_loaded_plugin(&io, SD("name", ""));
    } else if (!strcmp(func, "from_manifest_on_disk")) {
        r = pc2_from_manifest_on_disk(&io, SD("name", ""));
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