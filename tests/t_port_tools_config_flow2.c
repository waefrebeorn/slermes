/* t_port_tools_config_flow2.c — harness for the tools_config.py UNCLAIMED
 * FLOW port (src/cli/tools_config_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func","in","out":{"ret","trace","state","stdout","stderr"}}.
 * Scripted I/O lives in in.io (seam-name -> {"ret","raise","state"} or a queue
 * LIST). "print" and "color" are AUTO cosmetic seams; every other seam is
 * FAIL-CLOSED: a missing script raises RuntimeError("unscripted seam: <name>").
 */
#include "hermes_json.h"
#include "cli/tools_config_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    json_t *io;
    json_t *cursors;
} tc2_script_ctx_t;

static json_t *tc2_script_next(tc2_script_ctx_t *sc, const char *name) {
    if (!sc || !sc->io) return NULL;
    json_t *entry = json_obj_get(sc->io, name);
    if (!entry) return NULL;
    if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) return NULL;
        json_set(sc->cursors, name, json_number(cur + 1));
        return json_get(entry, idx);
    }
    return entry;
}

static void tc2_apply_state_effects(json_t *state, json_t *result) {
    if (!result || result->type != JSON_OBJECT) return;
    json_t *eff = json_obj_get(result, "state");
    if (!eff || eff->type != JSON_OBJECT) return;
    for (size_t i = 0; i < eff->c.count; i++)
        json_set(state, eff->c.keys[i], json_copy(eff->c.items[i]));
}

static json_t *tc2_dispatch(tc2_flow_io_t *io, const char *name, json_t *args) {
    tc2_script_ctx_t *sc = (tc2_script_ctx_t *)io->ctx;
    if (strcmp(name, "print") == 0) {
        const char *msg = args ? json_get_str(args, "text", "") : "";
        const char *file = args ? json_get_str(args, "file", "") : "";
        json_t *cap = (strcmp(file, "stderr") == 0) ? io->cap_stderr : io->cap_stdout;
        if (cap && cap->type == JSON_ARRAY) json_append(cap, json_string(msg));
        return json_null();
    }
    if (strcmp(name, "color") == 0) {
        const char *text = args ? json_get_str(args, "text", "") : "";
        return json_string(text);
    }
    json_t *result = tc2_script_next(sc, name);
    if (!result) {
        char *m = tc2_join("unscripted seam: ", name);
        json_t *r = tc2_make_raise("RuntimeError", m);
        free(m);
        return r;
    }
    tc2_apply_state_effects(io->state, result);
    if (result->type == JSON_OBJECT) {
        json_t *raise = json_obj_get(result, "raise");
        if (raise && raise->type == JSON_OBJECT) {
            json_t *out = json_object();
            json_set(out, "__raise__", json_copy(raise));
            return out;
        }
        json_t *ret = json_obj_get(result, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return json_copy(result);
}

static void tc2_io_init(tc2_flow_io_t *io, tc2_script_ctx_t *sc, json_t *in, json_t *state) {
    io->call = tc2_dispatch;
    io->trace = json_array();
    io->state = state;
    io->cap_stdout = json_array();
    io->cap_stderr = json_array();
    io->ctx = sc;
    sc->io = json_obj_get(in, "io");
    sc->cursors = json_object();
}

static json_t *tc2_state_from(json_t *in) {
    json_t *s = json_obj_get(in, "state");
    return (s && s->type == JSON_OBJECT) ? json_copy(s) : json_object();
}

static json_t *tc2_state_proj(json_t *state, json_t *in) {
    json_t *out = json_object();
    json_t *seed = json_obj_get(in, "state");
    if (seed && seed->type == JSON_OBJECT) {
        for (size_t i = 0; i < seed->c.count; i++) {
            json_t *v = json_obj_get(state, seed->c.keys[i]);
            json_set(out, seed->c.keys[i], v ? json_copy(v) : json_null());
        }
    }
    return out;
}

static json_t *tc2_run_case(const char *func, json_t *in) {
    tc2_flow_io_t io;
    tc2_script_ctx_t sc;
    json_t *state = tc2_state_from(in);
    tc2_io_init(&io, &sc, in, state);
    json_t *constants = json_obj_get(in, "constants");
    if (!constants) constants = json_object();
    json_t *ret = NULL;
    json_t *cfg = json_obj_get(in, "config");
    if (!cfg) cfg = json_object();

    if (strcmp(func, "_coerce_platform_toolsets_value") == 0) {
        ret = tc2_coerce(&io, json_obj_get(in, "value"), json_get_str(in, "platform", ""));
    } else if (strcmp(func, "_platform_toolsets_explicitly_saved") == 0) {
        ret = tc2_platform_toolsets_explicitly_saved(&io, cfg, json_get_str(in, "platform", ""));
    } else if (strcmp(func, "_default_off_toolsets") == 0) {
        ret = tc2_default_off_toolsets(&io, json_get_str(in, "platform", ""),
                    json_get_bool(in, "explicitly_configured", false), constants);
    } else if (strcmp(func, "_configurable_subset_of") == 0) {
        ret = tc2_configurable_subset_of(&io, json_obj_get(in, "tool_names"),
                    json_get_str(in, "platform", ""), constants);
    } else if (strcmp(func, "_composite_toolsets") == 0) {
        ret = tc2_composite_toolsets(&io, json_obj_get(in, "toolset_names"),
                    json_get_str(in, "platform", ""),
                    json_get_bool(in, "explicitly_configured", false), constants);
    } else if (strcmp(func, "_enabled_plugin_toolsets") == 0) {
        ret = tc2_enabled_plugin_toolsets(&io, cfg, json_get_str(in, "platform", ""),
                    json_obj_get(in, "toolset_names"), json_obj_get(in, "plugin_ts_keys"), constants);
    } else if (strcmp(func, "_context_engine_active") == 0) {
        ret = tc2_context_engine_active(&io, cfg);
    } else if (strcmp(func, "_merge_mcp_servers") == 0) {
        ret = tc2_merge_mcp_servers(&io, cfg, json_obj_get(in, "toolset_names"),
                    json_obj_get(in, "explicit_passthrough"),
                    json_get_bool(in, "include_default_mcp_servers", false));
    } else if (strcmp(func, "_prune_toolsets_stripped_by_disabled") == 0) {
        ret = tc2_prune_toolsets_stripped_by_disabled(&io, json_obj_get(in, "enabled_toolsets"),
                    json_obj_get(in, "disabled_names"));
    } else if (strcmp(func, "_disabled_entry_tools") == 0) {
        ret = tc2_disabled_entry_tools(&io, json_get_str(in, "name", ""));
    } else if (strcmp(func, "_platform_default_toolset") == 0) {
        ret = tc2_platform_default_toolset(&io, json_get_str(in, "platform", ""), constants);
    } else if (strcmp(func, "_is_configurable") == 0) {
        ret = tc2_is_configurable(&io, json_get_str(in, "ts_key", ""), constants);
    } else if (strcmp(func, "_toolset_label") == 0) {
        ret = tc2_toolset_label(&io, json_get_str(in, "ts_key", ""));
    } else if (strcmp(func, "_configurable_keys") == 0) {
        ret = tc2_configurable_keys(&io, constants);
    } else if (strcmp(func, "_platform_default_keys") == 0) {
        ret = tc2_platform_default_keys(&io, constants);
    } else if (strcmp(func, "_provider_env_ready") == 0) {
        ret = tc2_provider_env_ready(&io, json_obj_get(in, "provider"));
    } else if (strcmp(func, "_projected_plugin_toolsets") == 0) {
        ret = tc2_projected_plugin_toolsets(&io);
    } else if (strcmp(func, "_print_toolset_diff") == 0) {
        ret = tc2_print_toolset_diff(&io, json_obj_get(in, "added"),
                    json_obj_get(in, "removed"), json_get_str(in, "indent", "  "));
    } else if (strcmp(func, "_checklist_diff") == 0) {
        ret = tc2_checklist_diff(&io, json_obj_get(in, "new_enabled"),
                    json_obj_get(in, "prev"), json_get_str(in, "platform", ""));
    } else if (strcmp(func, "_current_platform_tools") == 0) {
        ret = tc2_current_platform_tools(&io, cfg, json_get_str(in, "pkey", ""));
    } else if (strcmp(func, "_toolsets_needing_setup") == 0) {
        ret = tc2_toolsets_needing_setup(&io, json_obj_get(in, "new_enabled"), cfg);
    } else if (strcmp(func, "_configure_newly_added") == 0) {
        ret = tc2_configure_newly_added(&io, json_obj_get(in, "added"),
                    json_obj_get(in, "already"), cfg);
    } else {
        ret = json_object();
        json_set(ret, "error", json_string("unknown func"));
    }

    json_t *out = json_object();
    if (tc2_raised(ret)) {
        json_t *spec = json_copy(tc2_raise_spec(ret));
        json_free(ret);
        ret = json_object();
        json_set(ret, "__raise__", spec);
    }
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", json_copy(io.trace));
    json_set(out, "state", tc2_state_proj(state, in));
    json_set(out, "stdout", json_copy(io.cap_stdout));
    json_set(out, "stderr", json_copy(io.cap_stderr));
    json_free(io.trace);
    json_free(io.cap_stdout);
    json_free(io.cap_stderr);
    json_free(sc.cursors);
    json_free(state);
    return out;
}

int main(void) {
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    char line[1 << 20];
    while (fgets(line, sizeof line, stdin)) {
        if (line[0] != '{') continue;
        char *err = NULL;
        json_t *rec = json_parse(line, &err);
        if (!rec) { free(err); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in) in = json_object();
        json_t *out = tc2_run_case(func, in);
        json_t *line_out = json_object();
        json_set(line_out, "func", json_string(func));
        json_set(line_out, "in", json_copy(in));
        json_set(line_out, "out", out);
        char *ser = json_serialize(line_out);
        printf("%s\n", ser ? ser : "{}");
        free(ser);
        json_free(line_out);
        json_free(rec);
    }
    return 0;
}
