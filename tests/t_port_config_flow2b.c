/* t_port_config_flow2b.c — harness for the config.py second-wave UNCLAIMED
 * FLOW port (src/cli/config_flow2b.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func","in","out":{"ret","trace","state","stdout","stderr"}}.
 * "trace" is the ordered seam-call sequence; "state" the mutated module-global
 * mirror (restricted to the keys the case seeded); "stdout"/"stderr" the
 * captured print messages routed by the print seam's args.file.
 *
 * Scripted I/O lives in in.io (seam-name -> {"ret","raise","import_raise","state"}
 * or a queue LIST). "print" and "color" are AUTO cosmetic seams; every other
 * seam is FAIL-CLOSED: a missing script raises RuntimeError("unscripted seam:
 * <name>"). An "import_raise" entry models a failed in-function import.
 */
#include "hermes_json.h"
#include "cli/config_flow2b.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    json_t *io;
    json_t *cursors;
} cfg2b_script_ctx_t;

static json_t *cfg2b_script_next(cfg2b_script_ctx_t *sc, const char *name) {
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

static void cfg2b_apply_state_effects(json_t *state, json_t *result) {
    if (!result || result->type != JSON_OBJECT) return;
    json_t *eff = json_obj_get(result, "state");
    if (!eff || eff->type != JSON_OBJECT) return;
    for (size_t i = 0; i < eff->c.count; i++)
        json_set(state, eff->c.keys[i], json_copy(eff->c.items[i]));
}

static json_t *cfg2b_dispatch(cfg2b_flow_io_t *io, const char *name, json_t *args) {
    cfg2b_script_ctx_t *sc = (cfg2b_script_ctx_t *)io->ctx;
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
    json_t *result = cfg2b_script_next(sc, name);
    if (!result) {
        char *m = cfg2b_join("unscripted seam: ", name);
        json_t *r = cfg2b_make_raise("RuntimeError", m);
        free(m);
        return r;
    }
    cfg2b_apply_state_effects(io->state, result);
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

static void cfg2b_io_init(cfg2b_flow_io_t *io, cfg2b_script_ctx_t *sc, json_t *in, json_t *state) {
    io->call = cfg2b_dispatch;
    io->trace = json_array();
    io->state = state;
    io->cap_stdout = json_array();
    io->cap_stderr = json_array();
    io->ctx = sc;
    sc->io = json_obj_get(in, "io");
    sc->cursors = json_object();
}

static json_t *cfg2b_state_from(json_t *in) {
    json_t *s = json_obj_get(in, "state");
    return (s && s->type == JSON_OBJECT) ? json_copy(s) : json_object();
}

static json_t *cfg2b_state_proj(json_t *state, json_t *in) {
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

static json_t *cfg2b_run_case(const char *func, json_t *in) {
    cfg2b_flow_io_t io;
    cfg2b_script_ctx_t sc;
    json_t *state = cfg2b_state_from(in);
    cfg2b_io_init(&io, &sc, in, state);
    json_t *ret = NULL;
    json_t *constants = json_obj_get(in, "constants");
    if (!constants) constants = json_object();

    if (strcmp(func, "warn_invalid_platform_toolsets") == 0) {
        ret = cfg2b_warn_invalid_platform_toolsets(&io, json_get_bool(in, "quiet", false));
    } else if (strcmp(func, "offer_new_optional_env_vars") == 0) {
        ret = cfg2b_offer_new_optional_env_vars(&io, json_get_num(in, "current_ver", 0),
                    json_get_num(in, "latest_ver", 0), constants);
    } else if (strcmp(func, "offer_skill_config_vars") == 0) {
        ret = cfg2b_offer_skill_config_vars(&io, json_obj_get(in, "missing_skill_config"), constants);
    } else if (strcmp(func, "note_policy") == 0) {
        ret = cfg2b_note_policy(&io, json_get_str(in, "setter", ""), json_obj_get(in, "raw"),
                                json_obj_get(in, "redact"));
    } else if (strcmp(func, "read_env_lines") == 0) {
        ret = cfg2b_read_env_lines(&io, json_get_str(in, "env_path", ""));
    } else if (strcmp(func, "env_write_lock_holder") == 0) {
        ret = cfg2b_env_write_lock_holder(&io, json_get_str(in, "env_path", ""));
    } else if (strcmp(func, "env_write_lock") == 0) {
        ret = cfg2b_env_write_lock(&io, json_get_str(in, "env_path", ""));
    } else if (strcmp(func, "write_env_lines") == 0) {
        ret = cfg2b_write_env_lines(&io, json_get_str(in, "env_path", ""),
                    json_obj_get(in, "lines"), json_get_bool(in, "preserve_mode", false));
    } else if (strcmp(func, "cmd_config_migrate") == 0) {
        ret = cfg2b_cmd_config_migrate(&io);
    } else if (strcmp(func, "cmd_config_check") == 0) {
        ret = cfg2b_cmd_config_check(&io, constants);
    } else if (strcmp(func, "platform_manifest_paths") == 0) {
        ret = cfg2b_platform_manifest_paths(&io, json_get_str(in, "home", NULL),
                    json_get_str(in, "source", "all"));
    } else if (strcmp(func, "manifest_stamp") == 0) {
        ret = cfg2b_manifest_stamp(&io, json_get_str(in, "home", NULL));
    } else if (strcmp(func, "platform_plugin_manifests") == 0) {
        json_t *sk = cfg2b_state_get(&io, "skipped");
        ret = cfg2b_platform_plugin_manifests(&io, json_get_str(in, "home", NULL),
                    json_get_str(in, "source", "all"), json_get_bool(in, "strict", false),
                    (sk && sk->type == JSON_ARRAY) ? sk : NULL);
    } else {
        ret = json_object();
        json_set(ret, "error", json_string("unknown func"));
    }

    json_t *out = json_object();
    if (cfg2b_raised(ret)) {
        json_t *spec = json_copy(cfg2b_raise_spec(ret));
        json_free(ret);
        ret = json_object();
        json_set(ret, "__raise__", spec);
    }
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", json_copy(io.trace));
    json_set(out, "state", cfg2b_state_proj(state, in));
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
    setvbuf(stdout, NULL, _IONBF, 0);
    char line[1 << 20];
    while (fgets(line, sizeof line, stdin)) {
        if (line[0] != '{') continue;
        char *err = NULL;
        json_t *rec = json_parse(line, &err);
        if (!rec) { free(err); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in) in = json_object();
        json_t *out = cfg2b_run_case(func, in);
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
