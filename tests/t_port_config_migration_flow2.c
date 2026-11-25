/* t_port_config_migration_flow2.c — harness for the config-migration registry
 * UNCLAIMED-remainder FLOW port (src/cli/config_migration_flow2.c):
 * hermes_cli/config_migrations.py steps 34/38/39 + the registered browser
 * sibling hermes_cli/config_migrations_browser.py (9 defs).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func","in","out":{"ret","trace","state","results","stdout"
 * [, "browser" for move_agent_browser_env | "config" for adopt_agent_browser_env]}}.
 * "trace" is the ordered seam-name sequence; "state" the mutated raw_config
 * mirror (restricted to the keys the case seeded); "results" the mutated caller
 * results dict; "stdout" the captured print messages.
 *
 * Scripted I/O lives in in.io (seam-name -> {"ret","state","raise","import_raise"}
 * or a queue LIST). AUTO seams: "print" (captures args.text; a scripted raise
 * fires), "read_raw_config" / "config.read_raw_config" (state-backed; ret/raise
 * override), "persist_migration" (state-backed; raise override) and "commit"
 * (models the claimed _commit: persist args.config + results["config_added"]
 * append + conditional stdout capture; raise override; KeyError/AttributeError
 * on a missing/non-list config_added). Every other seam is FAIL-CLOSED: a
 * missing/exhausted script raises RuntimeError("unscripted seam: <name>"). An
 * "import_raise" entry models a failed in-function import (untrace marker).
 * A scripted KeyError message is the RAW key (repr'd here to match
 * str(KeyError(k))).
 */
#include "hermes_json.h"
#include "cli/config_migration_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    json_t *io;
    json_t *cursors;
    json_t *results;   /* caller results dict (commit seam appends here) */
} cm2_script_ctx_t;

static json_t *cm2_script_peek(cm2_script_ctx_t *sc, const char *name) {
    if (!sc || !sc->io) return NULL;
    json_t *entry = json_obj_get(sc->io, name);
    if (!entry || entry->type == JSON_NULL) return NULL;
    if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) return NULL;
        return json_get(entry, idx);
    }
    return entry;
}
static void cm2_script_consume(cm2_script_ctx_t *sc, const char *name) {
    if (!sc || !sc->io) return;
    json_t *entry = json_obj_get(sc->io, name);
    if (entry && entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        json_set(sc->cursors, name, json_number(cur + 1));
    }
}
static json_t *cm2_script_next(cm2_script_ctx_t *sc, const char *name) {
    json_t *e = cm2_script_peek(sc, name);
    if (e) cm2_script_consume(sc, name);
    return e;
}

/* scripted raise spec -> {"__raise__": spec}; KeyError message repr'd. */
static json_t *cm2_norm_spec(const json_t *spec) {
    json_t *s = json_copy(spec);
    const char *type = json_get_str(s, "type", "");
    if (strcmp(type, "KeyError") == 0) {
        const char *msg = json_get_str(s, "message", "");
        char *rk = cm2_repr_str(msg);
        json_set(s, "message", json_string(rk));
        free(rk);
    }
    return s;
}
static json_t *cm2_raise_from(const json_t *spec) {
    json_t *out = json_object();
    json_set(out, "__raise__", cm2_norm_spec(spec));
    return out;
}

static json_t *cm2_dispatch(cm2_flow_io_t *io, const char *name, json_t *args) {
    cm2_script_ctx_t *sc = (cm2_script_ctx_t *)io->ctx;
    /* in-function import check (no trace token on success or failure). */
    if (args && json_obj_get(args, "__import__")) {
        json_t *entry = cm2_script_peek(sc, name);
        if (entry && entry->type == JSON_OBJECT) {
            json_t *imp = json_obj_get(entry, "import_raise");
            if (imp && imp->type == JSON_OBJECT) {
                cm2_script_consume(sc, name);
                json_t *out = json_object();
                json_set(out, "__raise__", cm2_norm_spec(imp));
                json_set(out, "__untrace__", json_bool(true));
                return out;
            }
        }
        json_t *out = json_object();
        json_set(out, "__import_ok__", json_bool(true));
        return out;
    }
    /* AUTO print seam (scripted raise still fires). */
    if (strcmp(name, "print") == 0) {
        json_t *entry = cm2_script_peek(sc, name);
        if (entry && entry->type == JSON_OBJECT) {
            json_t *raise = json_obj_get(entry, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                cm2_script_consume(sc, name);
                return cm2_raise_from(raise);
            }
        }
        cm2_script_consume(sc, name);
        const char *msg = args ? json_get_str(args, "text", "") : "";
        if (io->cap_stdout && io->cap_stdout->type == JSON_ARRAY)
            json_append(io->cap_stdout, json_string(msg));
        return json_null();
    }
    /* state-backed read_raw_config (both live bindings). */
    if (strcmp(name, "read_raw_config") == 0 || strcmp(name, "config.read_raw_config") == 0) {
        json_t *entry = cm2_script_next(sc, name);
        if (entry && entry->type == JSON_OBJECT) {
            json_t *raise = json_obj_get(entry, "raise");
            if (raise && raise->type == JSON_OBJECT) return cm2_raise_from(raise);
            if (json_has(entry, "ret")) return json_copy(json_obj_get(entry, "ret"));
        }
        json_t *rc = cm2_state_get(io, "raw_config");
        return rc ? json_copy(rc) : json_object();
    }
    /* state-backed persist_migration. */
    if (strcmp(name, "persist_migration") == 0) {
        json_t *entry = cm2_script_next(sc, name);
        if (entry && entry->type == JSON_OBJECT) {
            json_t *raise = json_obj_get(entry, "raise");
            if (raise && raise->type == JSON_OBJECT) return cm2_raise_from(raise);
        }
        json_t *cfg = args ? json_obj_get(args, "config") : NULL;
        json_set(io->state, "raw_config", cfg ? json_copy(cfg) : json_null());
        return json_null();
    }
    /* AUTO effect seam modelling the claimed _commit. */
    if (strcmp(name, "commit") == 0) {
        json_t *entry = cm2_script_peek(sc, name);
        if (entry && entry->type == JSON_OBJECT) {
            json_t *raise = json_obj_get(entry, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                cm2_script_consume(sc, name);
                return cm2_raise_from(raise);
            }
        }
        cm2_script_consume(sc, name);
        json_t *cfg = args ? json_obj_get(args, "config") : NULL;
        json_set(io->state, "raw_config", cfg ? json_copy(cfg) : json_null());
        json_t *added = args ? json_obj_get(args, "added") : NULL;
        if (cm2_truthy(added)) {
            json_t *arr = sc->results ? json_obj_get(sc->results, "config_added") : NULL;
            if (!arr) return cm2_key_raise("config_added");
            if (arr->type != JSON_ARRAY) return cm2_attr_raise(arr, "append");
            json_append(arr, json_copy(added));
        }
        json_t *message = args ? json_obj_get(args, "message") : NULL;
        bool quiet = args ? json_get_bool(args, "quiet", false) : false;
        if (cm2_truthy(message) && !quiet) {
            if (io->cap_stdout && io->cap_stdout->type == JSON_ARRAY)
                json_append(io->cap_stdout, json_string(cm2_sval(message)));
        }
        return json_null();
    }
    /* SCRIPTED fail-closed seams. */
    json_t *entry = cm2_script_next(sc, name);
    if (!entry) {
        char *m = cm2_join("unscripted seam: ", name);
        json_t *r = cm2_make_raise("RuntimeError", m);
        free(m);
        return r;
    }
    if (entry->type == JSON_OBJECT) {
        json_t *eff = json_obj_get(entry, "state");
        if (eff && eff->type == JSON_OBJECT)
            for (size_t i = 0; i < eff->c.count; i++)
                json_set(io->state, eff->c.keys[i], json_copy(eff->c.items[i]));
        json_t *imp = json_obj_get(entry, "import_raise");
        if (imp && imp->type == JSON_OBJECT) {
            json_t *out = json_object();
            json_set(out, "__raise__", cm2_norm_spec(imp));
            json_set(out, "__untrace__", json_bool(true));
            return out;
        }
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return cm2_raise_from(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return json_copy(entry);
}

static json_t *cm2_state_from(json_t *in) {
    json_t *s = json_obj_get(in, "state");
    return (s && s->type == JSON_OBJECT) ? json_copy(s) : json_object();
}
static json_t *cm2_state_proj(json_t *state, json_t *in) {
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
static json_t *cm2_results_from(json_t *in) {
    json_t *r = json_obj_get(in, "results");
    return (r && r->type == JSON_OBJECT) ? json_copy(r) : json_object();
}
static json_t *cm2_obj_from(json_t *in, const char *key) {
    json_t *c = json_obj_get(in, key);
    return (c && c->type == JSON_OBJECT) ? json_copy(c) : json_object();
}

static json_t *cm2_run_case(const char *func, json_t *in) {
    cm2_flow_io_t io;
    cm2_script_ctx_t sc;
    json_t *state = cm2_state_from(in);
    json_t *results = cm2_results_from(in);
    io.call = cm2_dispatch;
    io.trace = json_array();
    io.state = state;
    io.cap_stdout = json_array();
    io.ctx = &sc;
    sc.io = json_obj_get(in, "io");
    sc.cursors = json_object();
    sc.results = results;
    bool quiet = json_get_bool(in, "quiet", false);
    const char *home = json_get_str(in, "home", "");
    json_t *ret = NULL;
    json_t *browser_echo = NULL;
    json_t *config_echo = NULL;

    if (strcmp(func, "migrate_to_34") == 0) {
        ret = cm2_migrate_to_34(&io, results, quiet);
    } else if (strcmp(func, "migrate_to_38") == 0) {
        ret = cm2_migrate_to_38(&io, results, quiet);
    } else if (strcmp(func, "migrate_to_39") == 0) {
        ret = cm2_migrate_to_39(&io, results, quiet);
    } else if (strcmp(func, "agent_browser_env") == 0) {
        ret = cm2_agent_browser_env(&io, home);
    } else if (strcmp(func, "inside_pm_store") == 0) {
        ret = cm2_inside_pm_store(&io, json_get_str(in, "value", ""));
    } else if (strcmp(func, "browser_value_from_env") == 0) {
        ret = cm2_browser_value_from_env(&io, json_get_str(in, "key", ""),
                                         json_get_str(in, "value", ""));
    } else if (strcmp(func, "config_decided") == 0) {
        ret = cm2_config_decided(json_get_str(in, "key", ""), json_obj_get(in, "current"));
    } else if (strcmp(func, "backend_removed_note") == 0) {
        ret = cm2_backend_removed_note(json_obj_get(in, "backend"));
    } else if (strcmp(func, "move_agent_browser_env") == 0) {
        json_t *browser = cm2_obj_from(in, "browser");
        ret = cm2_move_agent_browser_env(&io, home, browser);
        browser_echo = browser;   /* mutated in place; echoed below */
    } else if (strcmp(func, "agent_browser_env_pending") == 0) {
        ret = cm2_agent_browser_env_pending(&io);
    } else if (strcmp(func, "adopt_agent_browser_env") == 0) {
        json_t *config = cm2_obj_from(in, "config");
        ret = cm2_adopt_agent_browser_env(&io, home, config);
        config_echo = config;   /* mutated in place; echoed below */
    } else if (strcmp(func, "migrate_to_52") == 0) {
        ret = cm2_migrate_to_52(&io, results, quiet);
    } else {
        ret = json_object();
        json_set(ret, "error", json_string("unknown func"));
    }

    json_t *out = json_object();
    /* an import-check raise that propagates out carries the internal __untrace__
     * marker; the live exception spec has no such field — strip it at the boundary. */
    if (ret && ret->type == JSON_OBJECT && json_obj_get(ret, "__untrace__"))
        json_obj_del(ret, "__untrace__");
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", json_copy(io.trace));
    json_set(out, "state", cm2_state_proj(state, in));
    json_set(out, "results", results);
    json_set(out, "stdout", json_copy(io.cap_stdout));
    if (browser_echo) json_set(out, "browser", browser_echo);
    if (config_echo) json_set(out, "config", config_echo);
    json_free(io.trace);
    json_free(io.cap_stdout);
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
        json_t *out = cm2_run_case(func, in);
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