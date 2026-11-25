/* t_port_config_flow2.c — harness for the hermes_cli/config.py UNCLAIMED
 * remainder FLOW port (src/cli/config_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func","in","out":{"ret","trace","state","stdout","stderr"}}.
 * "trace" is the ordered seam-call sequence; "state" the mutated module-global
 * mirror (restricted to the keys the case seeded); "stdout"/"stderr" the
 * captured print messages routed by the print seam's args.file.
 *
 * Scripted I/O lives in in.io (seam-name -> {"ret","state","raise","import_raise"}
 * or a queue LIST). Two cosmetic seams are AUTO: "print" (captures args.text)
 * and "color" (returns args.text). Every other seam is FAIL-CLOSED: a missing
 * script raises RuntimeError("unscripted seam: <name>"). An "import_raise"
 * entry models a failed in-function import (untrace marker, flag preserved).
 */
#include "hermes_json.h"
#include "cli/config_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    json_t *io;
    json_t *cursors;
} cfg2_script_ctx_t;

static json_t *cfg2_script_next(cfg2_script_ctx_t *sc, const char *name) {
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

static void cfg2_apply_state_effects(json_t *state, json_t *result) {
    if (!result || result->type != JSON_OBJECT) return;
    json_t *eff = json_obj_get(result, "state");
    if (!eff || eff->type != JSON_OBJECT) return;
    for (size_t i = 0; i < eff->c.count; i++)
        json_set(state, eff->c.keys[i], json_copy(eff->c.items[i]));
}

static json_t *cfg2_dispatch(cfg2_flow_io_t *io, const char *name, json_t *args) {
    cfg2_script_ctx_t *sc = (cfg2_script_ctx_t *)io->ctx;
    /* AUTO cosmetic seams (no script, never fail closed). */
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
    /* SCRIPTED fail-closed seams. */
    json_t *result = cfg2_script_next(sc, name);
    if (!result) {
        char *m = cfg2_join("unscripted seam: ", name);
        json_t *r = cfg2_make_raise("RuntimeError", m);
        free(m);
        return r;
    }
    cfg2_apply_state_effects(io->state, result);
    if (result->type == JSON_OBJECT) {
        json_t *imp = json_obj_get(result, "import_raise");
        if (imp && imp->type == JSON_OBJECT) {
            json_t *out = json_object();
            json_set(out, "__raise__", json_copy(imp));
            json_set(out, "__untrace__", json_bool(true));
            return out;
        }
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

static void cfg2_io_init(cfg2_flow_io_t *io, cfg2_script_ctx_t *sc, json_t *in, json_t *state) {
    io->call = cfg2_dispatch;
    io->trace = json_array();
    io->state = state;
    io->cap_stdout = json_array();
    io->cap_stderr = json_array();
    io->ctx = sc;
    sc->io = json_obj_get(in, "io");
    sc->cursors = json_object();
}

static json_t *cfg2_state_from(json_t *in) {
    json_t *s = json_obj_get(in, "state");
    return (s && s->type == JSON_OBJECT) ? json_copy(s) : json_object();
}

static json_t *cfg2_state_proj(json_t *state, json_t *in) {
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

/* build a NULL-terminated char* view of a JSON string array (caller frees). */
static const char **cfg2_str_array(json_t *arr, size_t *n_out) {
    size_t n = (arr && arr->type == JSON_ARRAY) ? json_len(arr) : 0;
    const char **v = malloc((n + 1) * sizeof(char *));
    for (size_t i = 0; i < n; i++) v[i] = cfg2_sval(json_get(arr, i));
    v[n] = NULL;
    *n_out = n;
    return v;
}

static json_t *cfg2_run_case(const char *func, json_t *in) {
    cfg2_flow_io_t io;
    cfg2_script_ctx_t sc;
    json_t *state = cfg2_state_from(in);
    cfg2_io_init(&io, &sc, in, state);
    json_t *ret = NULL;

    if (strcmp(func, "format_docker_update_message") == 0) {
        ret = cfg2_format_docker_update_message(&io);
    } else if (strcmp(func, "module_getattr") == 0) {
        ret = cfg2_module_getattr(&io, json_get_str(in, "name", ""));
    } else if (strcmp(func, "managed_source") == 0) {
        ret = cfg2_managed_source(&io, json_get_str(in, "filename", ""));
    } else if (strcmp(func, "env_write_refusal") == 0) {
        ret = cfg2_env_write_refusal(&io, json_get_str(in, "key", ""), json_get_str(in, "action", ""));
    } else if (strcmp(func, "env_write_blocked") == 0) {
        ret = cfg2_env_write_blocked(&io, json_get_str(in, "key", ""), json_get_str(in, "action", ""));
    } else if (strcmp(func, "require_env_writable") == 0) {
        ret = cfg2_require_env_writable(&io, json_get_str(in, "key", ""), json_get_str(in, "action", ""));
    } else if (strcmp(func, "write_anthropic_slots") == 0) {
        ret = cfg2_write_anthropic_slots(&io, json_get_str(in, "token", ""), json_get_str(in, "api_key", ""),
                                         json_obj_get(in, "save_fn"), json_get_bool(in, "token_first", true));
    } else if (strcmp(func, "scoped_environ_get") == 0) {
        ret = cfg2_scoped_environ_get(&io, json_get_str(in, "key", ""));
    } else if (strcmp(func, "legacy_gateway_platforms_key") == 0) {
        ret = cfg2_legacy_gateway_platforms_key(&io, json_get_str(in, "requested_key", ""));
    } else if (strcmp(func, "exit_if_key_managed") == 0) {
        ret = cfg2_exit_if_key_managed(&io, json_get_str(in, "key", ""), json_get_str(in, "action", ""));
    } else if (strcmp(func, "touch_skin_file") == 0) {
        ret = cfg2_touch_skin_file(&io, json_get_str(in, "key", ""), json_obj_get(in, "value"));
    } else if (strcmp(func, "exit_invalid") == 0) {
        ret = cfg2_exit_invalid(&io, json_get_str(in, "msg", ""));
    } else if (strcmp(func, "write_user_config") == 0) {
        ret = cfg2_write_user_config(&io, json_get_str(in, "config_path", ""), json_obj_get(in, "user_config"));
    } else if (strcmp(func, "print_unknown_key_notice") == 0) {
        ret = cfg2_print_unknown_key_notice(&io, json_get_str(in, "key", ""), json_get_str(in, "suggestion", NULL));
    } else if (strcmp(func, "dropped_shadow_notes") == 0) {
        ret = cfg2_dropped_shadow_notes(&io, json_obj_get(state, "user_config"),
                                        json_get_str(in, "key", ""), json_get_str(in, "legacy_key", NULL));
    } else if (strcmp(func, "usage_exit") == 0) {
        size_t ne, nx;
        const char **ex = cfg2_str_array(json_obj_get(in, "examples"), &ne);
        const char **xt = cfg2_str_array(json_obj_get(in, "extra"), &nx);
        ret = cfg2_usage_exit(&io, json_get_str(in, "usage", ""), ex, xt);
        free(ex); free(xt);
    } else if (strcmp(func, "run_write_command") == 0) {
        json_t *a = json_obj_get(in, "args");
        ret = cfg2_run_write_command(&io, json_get_str(in, "fn_name", ""), a ? json_copy(a) : json_array());
    } else if (strcmp(func, "cmd_config_get") == 0) {
        ret = cfg2_cmd_config_get(&io, json_obj_get(in, "args"));
    } else if (strcmp(func, "cmd_config_set") == 0) {
        ret = cfg2_cmd_config_set(&io, json_obj_get(in, "args"));
    } else if (strcmp(func, "cmd_config_unset") == 0) {
        ret = cfg2_cmd_config_unset(&io, json_obj_get(in, "args"));
    } else if (strcmp(func, "tools_suffix") == 0) {
        ret = cfg2_tools_suffix(&io, json_obj_get(in, "info"), json_get_str(in, "fmt", ""));
    } else if (strcmp(func, "print_banner") == 0) {
        ret = cfg2_print_banner(&io, json_get_str(in, "text", ""));
    } else if (strcmp(func, "cmd_config_show") == 0) {
        ret = cfg2_cmd_config_show(&io);
    } else if (strcmp(func, "is_plugin_dir_name") == 0) {
        ret = cfg2_is_plugin_dir_name(&io, json_get_str(in, "name", ""));
    } else if (strcmp(func, "platform_manifest_env_entries") == 0) {
        ret = cfg2_platform_manifest_env_entries(&io, json_obj_get(in, "manifest"), json_get_bool(in, "optional", true));
    } else if (strcmp(func, "manifest_secret_envs") == 0) {
        ret = cfg2_manifest_secret_envs(&io, json_obj_get(in, "manifests"), json_obj_get(in, "core"));
    } else if (strcmp(func, "platform_manifest_secret_envs") == 0) {
        ret = cfg2_platform_manifest_secret_envs(&io, json_get_str(in, "home", NULL),
                                                 json_get_str(in, "source", "user"), json_get_bool(in, "strict", false));
    } else if (strcmp(func, "platform_manifest_secret_scan") == 0) {
        ret = cfg2_platform_manifest_secret_scan(&io, json_get_str(in, "home", NULL));
    } else if (strcmp(func, "ask_yes_no") == 0) {
        ret = cfg2_ask_yes_no(&io, json_get_str(in, "prompt", ""));
    } else if (strcmp(func, "offer_list") == 0) {
        ret = cfg2_offer_list(&io, json_get_str(in, "heading", ""), json_obj_get(in, "items"),
                              json_get_str(in, "question", ""));
    } else if (strcmp(func, "prompt_and_save_env") == 0) {
        ret = cfg2_prompt_and_save_env(&io, json_get_str(in, "name", ""), json_obj_get(in, "info"),
                                       json_get_str(in, "prompt", ""), json_obj_get(state, "results"));
    } else {
        ret = json_object();
        json_set(ret, "error", json_string("unknown func"));
    }

    json_t *out = json_object();
    /* Normalize a propagated raise: drop the internal __untrace__ import flag
     * so the envelope matches the live exc_spec shape ({__raise__:{...}} only). */
    if (cfg2_raised(ret)) {
        json_t *spec = json_copy(cfg2_raise_spec(ret));
        json_free(ret);
        ret = json_object();
        json_set(ret, "__raise__", spec);
    }
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", json_copy(io.trace));
    json_set(out, "state", cfg2_state_proj(state, in));
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
        json_t *out = cfg2_run_case(func, in);
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