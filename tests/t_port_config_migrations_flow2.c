/* t_port_config_migrations_flow2.c — harness for the hermes_cli/config_migrations.py
 * UNCLAIMED-remainder FLOW port (src/cli/config_migrations_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func","in","out":{"ret","trace","state","results","stdout"}}.
 * "trace" is the ordered seam-name sequence; "state" the mutated raw_config
 * mirror (restricted to the keys the case seeded); "results" the mutated caller
 * results dict; "stdout" the captured print messages.
 *
 * Scripted I/O lives in in.io (seam-name -> {"ret","state","raise","import_raise"}
 * or a queue LIST). AUTO seams: "print" (captures args.text; a scripted raise
 * fires), "config.read_raw_config" (state-backed; ret/raise override) and
 * "config._persist_migration" (state-backed; raise override). Every other seam
 * is FAIL-CLOSED: a missing/exhausted script raises RuntimeError("unscripted
 * seam: <name>"). An "import_raise" entry models a failed in-function import
 * (untrace marker; flag preserved). A scripted KeyError message is the RAW key
 * (repr'd here to match str(KeyError(k))).
 */
#include "hermes_json.h"
#include "cli/config_migrations_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    json_t *io;
    json_t *cursors;
} cmg2_script_ctx_t;

static json_t *cmg2_script_peek(cmg2_script_ctx_t *sc, const char *name) {
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
static void cmg2_script_consume(cmg2_script_ctx_t *sc, const char *name) {
    if (!sc || !sc->io) return;
    json_t *entry = json_obj_get(sc->io, name);
    if (entry && entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        json_set(sc->cursors, name, json_number(cur + 1));
    }
}
static json_t *cmg2_script_next(cmg2_script_ctx_t *sc, const char *name) {
    json_t *e = cmg2_script_peek(sc, name);
    if (e) cmg2_script_consume(sc, name);
    return e;
}

/* scripted raise spec -> {"__raise__": spec}; KeyError message repr'd. */
static json_t *cmg2_norm_spec(const json_t *spec) {
    json_t *s = json_copy(spec);
    const char *type = json_get_str(s, "type", "");
    if (strcmp(type, "KeyError") == 0) {
        const char *msg = json_get_str(s, "message", "");
        char *rk = cmg2_repr_str(msg);
        json_set(s, "message", json_string(rk));
        free(rk);
    }
    return s;
}
static json_t *cmg2_raise_from(const json_t *spec) {
    json_t *out = json_object();
    json_set(out, "__raise__", cmg2_norm_spec(spec));
    return out;
}

static json_t *cmg2_dispatch(cmg2_flow_io_t *io, const char *name, json_t *args) {
    cmg2_script_ctx_t *sc = (cmg2_script_ctx_t *)io->ctx;
    /* in-function import check (no trace token on success or failure). */
    if (args && json_obj_get(args, "__import__")) {
        json_t *entry = cmg2_script_peek(sc, name);
        if (entry && entry->type == JSON_OBJECT) {
            json_t *imp = json_obj_get(entry, "import_raise");
            if (imp && imp->type == JSON_OBJECT) {
                cmg2_script_consume(sc, name);
                json_t *out = json_object();
                json_set(out, "__raise__", cmg2_norm_spec(imp));
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
        json_t *entry = cmg2_script_peek(sc, name);
        if (entry && entry->type == JSON_OBJECT) {
            json_t *raise = json_obj_get(entry, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                cmg2_script_consume(sc, name);
                return cmg2_raise_from(raise);
            }
        }
        cmg2_script_consume(sc, name);
        const char *msg = args ? json_get_str(args, "text", "") : "";
        if (io->cap_stdout && io->cap_stdout->type == JSON_ARRAY)
            json_append(io->cap_stdout, json_string(msg));
        return json_null();
    }
    /* state-backed config.read_raw_config */
    if (strcmp(name, "config.read_raw_config") == 0) {
        json_t *entry = cmg2_script_next(sc, name);
        if (entry && entry->type == JSON_OBJECT) {
            json_t *raise = json_obj_get(entry, "raise");
            if (raise && raise->type == JSON_OBJECT) return cmg2_raise_from(raise);
            if (json_has(entry, "ret")) return json_copy(json_obj_get(entry, "ret"));
        }
        json_t *rc = cmg2_state_get(io, "raw_config");
        return rc ? json_copy(rc) : json_object();
    }
    /* state-backed config._persist_migration */
    if (strcmp(name, "config._persist_migration") == 0) {
        json_t *entry = cmg2_script_next(sc, name);
        if (entry && entry->type == JSON_OBJECT) {
            json_t *raise = json_obj_get(entry, "raise");
            if (raise && raise->type == JSON_OBJECT) return cmg2_raise_from(raise);
        }
        json_t *cfg = args ? json_obj_get(args, "config") : NULL;
        json_set(io->state, "raw_config", cfg ? json_copy(cfg) : json_null());
        return json_null();
    }
    /* SCRIPTED fail-closed seams. */
    json_t *entry = cmg2_script_next(sc, name);
    if (!entry) {
        char *m = cmg2_join("unscripted seam: ", name);
        json_t *r = cmg2_make_raise("RuntimeError", m);
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
            json_set(out, "__raise__", cmg2_norm_spec(imp));
            json_set(out, "__untrace__", json_bool(true));
            return out;
        }
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return cmg2_raise_from(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return json_copy(entry);
}

static json_t *cmg2_state_from(json_t *in) {
    json_t *s = json_obj_get(in, "state");
    return (s && s->type == JSON_OBJECT) ? json_copy(s) : json_object();
}
static json_t *cmg2_state_proj(json_t *state, json_t *in) {
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
static json_t *cmg2_results_from(json_t *in) {
    json_t *r = json_obj_get(in, "results");
    return (r && r->type == JSON_OBJECT) ? json_copy(r) : json_object();
}
static json_t *cmg2_config_from(json_t *in) {
    json_t *c = json_obj_get(in, "config");
    return (c && c->type == JSON_OBJECT) ? json_copy(c) : json_object();
}
static const char *cmg2_str_or_null(json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static json_t *cmg2_run_case(const char *func, json_t *in) {
    cmg2_flow_io_t io;
    cmg2_script_ctx_t sc;
    json_t *state = cmg2_state_from(in);
    json_t *results = cmg2_results_from(in);
    io.call = cmg2_dispatch;
    io.trace = json_array();
    io.state = state;
    io.cap_stdout = json_array();
    io.ctx = &sc;
    sc.io = json_obj_get(in, "io");
    sc.cursors = json_object();
    bool quiet = json_get_bool(in, "quiet", false);
    json_t *ret = NULL;

    if (strcmp(func, "local_files_allowed") == 0) {
        ret = cmg2_local_files_allowed(&io);
    } else if (strcmp(func, "read_raw_config") == 0) {
        ret = cmg2_read_raw_config(&io);
    } else if (strcmp(func, "persist_migration") == 0) {
        json_t *config = cmg2_config_from(in);
        ret = cmg2_persist_migration(&io, config);
        json_free(config);
    } else if (strcmp(func, "dict_at") == 0) {
        json_t *config = cmg2_config_from(in);
        bool fresh = false;
        json_t *v = cmg2_dict_at(config, json_get_str(in, "key", ""), &fresh);
        ret = json_copy(v);
        if (fresh) json_free(v);
        json_free(config);
    } else if (strcmp(func, "commit") == 0) {
        json_t *config = cmg2_config_from(in);
        ret = cmg2_commit(&io, config, results, quiet,
                          cmg2_str_or_null(in, "added"), cmg2_str_or_null(in, "message"));
        json_free(config);
    } else if (strcmp(func, "rewrite_key") == 0) {
        ret = cmg2_rewrite_key(&io, results, quiet,
                               json_get_str(in, "section", ""), json_get_str(in, "key", ""),
                               json_obj_get(in, "match"), json_obj_get(in, "new"),
                               cmg2_str_or_null(in, "added"), cmg2_str_or_null(in, "message"),
                               json_obj_get(in, "extra_guard"),
                               json_get_bool(in, "create_section", false));
    } else if (strcmp(func, "rewrite_stale_default") == 0) {
        ret = cmg2_rewrite_stale_default(json_obj_get(in, "old"), json_obj_get(in, "kw"));
    } else if (strcmp(func, "rewrite_stale_defaults") == 0) {
        ret = cmg2_rewrite_stale_defaults(&io, results, quiet,
                                          json_get_str(in, "section", ""), json_obj_get(in, "rows"));
    } else if (strcmp(func, "lower_is") == 0) {
        ret = json_bool(cmg2_lower_is_match(json_get_str(in, "word", ""), json_obj_get(in, "cur")));
    } else if (strcmp(func, "installed_user_plugins") == 0) {
        ret = cmg2_installed_user_plugins(&io, json_obj_get(in, "disabled"));
    } else if (strcmp(func, "migrate_to_41") == 0) {
        ret = cmg2_migrate_to_41(&io, results, quiet);
    } else if (strcmp(func, "migrate_to_44") == 0) {
        ret = cmg2_migrate_to_44(&io, results, quiet);
    } else if (strcmp(func, "migrate_to_45") == 0) {
        ret = cmg2_migrate_to_45(&io, results, quiet);
    } else if (strcmp(func, "migrate_to_46") == 0) {
        ret = cmg2_migrate_to_46(&io, results, quiet);
    } else if (strcmp(func, "migrate_to_48") == 0) {
        ret = cmg2_migrate_to_48(&io, results, quiet);
    } else if (strcmp(func, "migrate_to_49") == 0) {
        ret = cmg2_migrate_to_49(&io, results, quiet);
    } else if (strcmp(func, "migrate_to_50") == 0) {
        ret = cmg2_migrate_to_50(&io, results, quiet);
    } else if (strcmp(func, "prompt_caching_child_lines") == 0) {
        ret = cmg2_prompt_caching_child_lines(json_obj_get(in, "text"));
    } else if (strcmp(func, "migrate_to_54") == 0) {
        ret = cmg2_migrate_to_54(&io, results, quiet);
    } else if (strcmp(func, "disable_suspicious_mcp_servers") == 0) {
        ret = cmg2_disable_suspicious_mcp_servers(&io, results, quiet);
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
    json_set(out, "state", cmg2_state_proj(state, in));
    json_set(out, "results", results);
    json_set(out, "stdout", json_copy(io.cap_stdout));
    json_free(io.trace);
    json_free(io.cap_stdout);
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
        json_t *out = cmg2_run_case(func, in);
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