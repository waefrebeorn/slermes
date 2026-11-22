/* t_port_config_migration_flow.c — harness for the config migration /
 * backend FLOW port (src/hermes_cli/config_migration_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out": {"ret","trace","state","config"?}}.
 * "trace" is the ordered seam-call sequence ({"name","args"} events);
 * "state" the mutated module-global mirror (restricted to the keys the case
 * seeded); "config" the mutated input config (only when the case seeded one).
 *
 * Scripted I/O lives in in.io (seam-name -> {"ret","state","raise",
 * "import_raise"} or a queue LIST), applied by the dispatcher exactly as the
 * oracle's recording stubs are fed. An "import_raise" entry models a failed
 * in-function import: the dispatcher returns an untrace marker so the silent
 * live import records nothing.
 */
#include "hermes_json.h"
#include "config_migration_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    json_t *io;
    json_t *cursors;
} cmf_script_ctx_t;

static json_t *cmf_script_next(cmf_script_ctx_t *sc, const char *name) {
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

static void cmf_apply_state_effects(json_t *state, json_t *result) {
    if (!result || result->type != JSON_OBJECT) return;
    json_t *eff = json_obj_get(result, "state");
    if (!eff || eff->type != JSON_OBJECT) return;
    for (size_t i = 0; i < eff->c.count; i++)
        json_set(state, eff->c.keys[i], json_copy(eff->c.items[i]));
}

static json_t *cmf_dispatch(cmf_io_t *io, const char *name, json_t *args) {
    (void)args;   /* args are traced by cmf_seam; scripts are keyed by name */
    cmf_script_ctx_t *sc = (cmf_script_ctx_t *)io->ctx;
    json_t *result = cmf_script_next(sc, name);
    cmf_apply_state_effects(io->state, result);
    if (result && result->type == JSON_OBJECT) {
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
    return json_null();
}

/* restrict the mutated state to the keys the case seeded (oracle does same) */
static json_t *cmf_state_proj(json_t *state, json_t *in) {
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

static const char *cmf_str_or_null(json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static json_t *cmf_config_from(json_t *in) {
    json_t *c = json_obj_get(in, "config");
    return (c && c->type == JSON_OBJECT) ? json_copy(c) : json_object();
}

static json_t *cmf_run_case(const char *func, json_t *in) {
    cmf_io_t io;
    cmf_script_ctx_t sc;
    json_t *state = json_obj_get(in, "state");
    state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
    io.call = cmf_dispatch;
    io.trace = json_array();
    io.state = state;
    io.ctx = &sc;
    sc.io = json_obj_get(in, "io");
    sc.cursors = json_object();

    json_t *ret = NULL;
    json_t *config_out = NULL;
    const cmf_feature_t *f = cmf_feature_lookup(cmf_str_or_null(in, "feature"));
    bool needs_feature =
        strcmp(func, "marked") == 0 || strcmp(func, "mark") == 0 ||
        strcmp(func, "core_carried") == 0 ||        strcmp(func, "predates_split") == 0 || strcmp(func, "turn_off_replaced") == 0 ||
        strcmp(func, "keep_legacy_disable") == 0 || strcmp(func, "record_migration") == 0 ||
        strcmp(func, "install_one") == 0;
    if (needs_feature && !f) {
        if (strcmp(func, "record_migration") != 0 && strcmp(func, "install_one") != 0)
            config_out = cmf_config_from(in);
        ret = cmf_make_raise("StopIteration", "");
    } else if (strcmp(func, "marked") == 0) {
        config_out = cmf_config_from(in);
        ret = json_bool(cmf_marked(config_out, json_get_str(in, "key", ""), f));
    } else if (strcmp(func, "mark") == 0) {
        config_out = cmf_config_from(in);
        cmf_mark(config_out, json_get_str(in, "key", ""), f);
        ret = json_null();
    } else if (strcmp(func, "core_carried") == 0) {
        ret = json_bool(cmf_core_carried(f, json_obj_get(in, "selection")));
    } else if (strcmp(func, "predates_split") == 0) {
        ret = cmf_predates_split(&io, json_get_str(in, "home", ""), f);
    } else if (strcmp(func, "record_core_use") == 0) {
        config_out = cmf_config_from(in);
        ret = cmf_record_core_use(&io, json_get_str(in, "home", ""), config_out,
                                  (int)json_get_num(in, "version", 0));
    } else if (strcmp(func, "turn_off_replaced") == 0) {
        config_out = cmf_config_from(in);
        ret = cmf_turn_off_replaced(&io, config_out, f);
    } else if (strcmp(func, "keep_legacy_disable") == 0) {
        config_out = cmf_config_from(in);
        ret = cmf_keep_legacy_disable(&io, config_out, f);
    } else if (strcmp(func, "record_migration") == 0) {
        ret = cmf_record_migration(&io, json_get_str(in, "home", ""), f);
    } else if (strcmp(func, "first_cause") == 0) {
        char *s = cmf_first_cause(json_get_str(in, "error", ""));
        ret = json_string(s ? s : "");
        free(s);
    } else if (strcmp(func, "install_one") == 0) {
        ret = cmf_install_one(&io, json_get_str(in, "home", ""), f,
                              json_get_bool(in, "process_env", false));
    } else if (strcmp(func, "migrate_home") == 0) {
        ret = cmf_migrate_home(&io, json_get_str(in, "home", ""),
                               json_get_bool(in, "process_env", false));
    } else if (strcmp(func, "version_token") == 0) {
        ret = cmf_version_token(&io, json_obj_get(in, "version"));
    } else if (strcmp(func, "backend_for") == 0) {
        ret = cmf_backend_for(&io, json_get_str(in, "kind", ""));
    } else if (strcmp(func, "get_config_backend") == 0) {
        ret = cmf_get_config_backend(&io);
    } else if (strcmp(func, "bootstrap") == 0) {
        ret = cmf_bootstrap(&io, cmf_str_or_null(in, "home"),
                            cmf_str_or_null(in, "project_env"),
                            json_get_bool(in, "once", false));
    } else if (strcmp(func, "ensure_bootstrap_env") == 0) {
        ret = cmf_ensure_bootstrap_env(&io);
    } else if (strcmp(func, "route") == 0) {
        json_t *r = cmf_route(&io, json_get_str(in, "config_path", ""));
        if (cmf_raised(r)) {
            ret = r;
        } else {
            ret = json_array();
            json_append(ret, json_copy(json_obj_get(r, "backend")));
            json_append(ret, json_string(json_get_str(r, "home", "")));
            json_free(r);
        }
    } else if (strcmp(func, "read_config_doc") == 0) {
        ret = cmf_read_config_doc_flow(&io, json_get_str(in, "config_path", ""));
    } else if (strcmp(func, "write_path") == 0) {
        ret = cmf_write_path(&io, json_get_str(in, "path", ""), json_obj_get(in, "changes"));
    } else if (strcmp(func, "config_cache_version_or_none") == 0) {
        ret = cmf_config_cache_version_or_none(&io, json_get_str(in, "config_path", ""));
    } else if (strcmp(func, "write_config_keys") == 0) {
        ret = cmf_write_config_keys(&io, json_get_str(in, "config_path", ""),
                                    json_obj_get(in, "updates"));
    } else {
        ret = json_object();
        json_set(ret, "error", json_string("unknown func"));
    }

    json_t *out = json_object();
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", json_copy(io.trace));
    json_set(out, "state", cmf_state_proj(state, in));
    if (config_out) json_set(out, "config", config_out);
    json_free(io.trace);
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
        json_t *out = cmf_run_case(func, in);
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