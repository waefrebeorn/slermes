/* t_port_config_load_save_flow.c — harness for the hermes_cli/config.py
 * load/migration/save FLOW port (src/hermes_cli/config_load_save_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out": {"ret","trace","state","stderr"}}.
 * "trace" is the ordered seam-call sequence; "state" the mutated module-global
 * mirror (restricted to the keys the case seeded); "stderr" the captured
 * print_stderr / exit_invalid messages.
 *
 * Scripted I/O lives in in.io (seam-name -> {"ret","state","raise","import_raise"}
 * or a queue LIST), applied by the dispatcher exactly as the oracle's recording
 * stubs are fed. An "import_raise" entry models a failed in-function import: the
 * dispatcher returns an untrace marker so the silent live import records nothing.
 */
#include "hermes_json.h"
#include "config_load_save_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    json_t *io;
    json_t *cursors;
} cfgls_script_ctx_t;

static json_t *cfgls_script_next(cfgls_script_ctx_t *sc, const char *name) {
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

static void cfgls_apply_state_effects(json_t *state, json_t *result) {
    if (!result || result->type != JSON_OBJECT) return;
    json_t *eff = json_obj_get(result, "state");
    if (!eff || eff->type != JSON_OBJECT) return;
    for (size_t i = 0; i < eff->c.count; i++)
        json_set(state, eff->c.keys[i], json_copy(eff->c.items[i]));
}

static json_t *cfgls_dispatch(cfgls_flow_io_t *io, const char *name, json_t *args) {
    cfgls_script_ctx_t *sc = (cfgls_script_ctx_t *)io->ctx;
    /* capture printed messages (print_stderr / exit_invalid) for comparison */
    if ((strcmp(name, "print_stderr") == 0 || strcmp(name, "exit_invalid") == 0) && args) {
        json_t *msg = json_obj_get(args, "message");
        if (msg && io->captured_stderr && io->captured_stderr->type == JSON_ARRAY)
            json_append(io->captured_stderr, json_copy(msg));
    }
    json_t *result = cfgls_script_next(sc, name);
    cfgls_apply_state_effects(io->state, result);
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

static void cfgls_io_init(cfgls_flow_io_t *io, cfgls_script_ctx_t *sc, json_t *in, json_t *state) {
    io->call = cfgls_dispatch;
    io->trace = json_array();
    io->state = state;
    io->captured_stderr = json_array();
    io->ctx = sc;
    sc->io = json_obj_get(in, "io");
    sc->cursors = json_object();
}

static json_t *cfgls_state_from(json_t *in) {
    json_t *s = json_obj_get(in, "state");
    return (s && s->type == JSON_OBJECT) ? json_copy(s) : json_object();
}

/* restrict the mutated state to the keys the case seeded (oracle does the same) */
static json_t *cfgls_state_proj(json_t *state, json_t *in) {
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

/* drive-level tuple(cache_sig/cache_key): the oracle converts before calling.
 * array -> copy; str -> chars; dict -> keys; scalar -> TypeError "'X' object
 * is not iterable". null_is_none: load_* pass None through; raw_* tuple(None)
 * raises (their drive default is []). Returns NULL for pass-through None, a
 * NEW array, or a raise node. */
static json_t *cfgls_sig_tuple(const json_t *cs, bool null_is_none) {
    if (!cs || cs->type == JSON_NULL) {
        if (null_is_none) return NULL;
        return cfgls_make_raise("TypeError", "'NoneType' object is not iterable");
    }
    if (cs->type == JSON_ARRAY) return json_copy(cs);
    json_t *out = json_array();
    if (cs->type == JSON_STRING) {
        const char *s = cs->str_val ? cs->str_val : "";
        for (size_t i = 0; s[i]; i++) {
            char ch[2] = { s[i], '\0' };
            json_append(out, json_string(ch));
        }
        return out;
    }
    if (cs->type == JSON_OBJECT) {
        for (size_t i = 0; i < cs->c.count; i++) json_append(out, json_string(cs->c.keys[i]));
        return out;
    }
    json_free(out);
    char m[128];
    snprintf(m, sizeof m, "'%s' object is not iterable", cfgls_py_type_name(cs));
    return cfgls_make_raise("TypeError", m);
}

static json_t *cfgls_run_case(const char *func, json_t *in) {
    cfgls_flow_io_t io;
    cfgls_script_ctx_t sc;
    json_t *state = cfgls_state_from(in);
    cfgls_io_init(&io, &sc, in, state);
    json_t *ret = NULL;

    if (strcmp(func, "worker_config_snapshot") == 0) {
        ret = cfgls_worker_config_snapshot(&io);
    } else if (strcmp(func, "raw_config_cache_hit") == 0) {
        json_t *ck = cfgls_sig_tuple(json_obj_get(in, "cache_key"), false);
        if (ck && cfgls_raised(ck)) ret = ck;
        else {
            ret = cfgls_raw_config_cache_hit(&io, json_get_str(in, "path_key", ""), ck);
            json_free(ck);
        }
    } else if (strcmp(func, "raw_config_cache_store") == 0) {
        json_t *ck = cfgls_sig_tuple(json_obj_get(in, "cache_key"), false);
        if (ck && cfgls_raised(ck)) ret = ck;
        else {
            ret = cfgls_raw_config_cache_store(&io, json_get_str(in, "path_key", ""),
                                               ck, json_obj_get(in, "data"));
            json_free(ck);
        }
    } else if (strcmp(func, "read_raw_config_impl") == 0) {
        ret = cfgls_read_raw_config_impl_flow(&io, json_get_bool(in, "want_deepcopy", true));
    } else if (strcmp(func, "load_config_cache_sig") == 0) {
        ret = cfgls_load_config_cache_sig(&io, json_get_str(in, "config_path", ""));
    } else if (strcmp(func, "load_config_cache_hit") == 0) {
        json_t *cs = cfgls_sig_tuple(json_obj_get(in, "cache_sig"), true);
        if (cs && cfgls_raised(cs)) ret = cs;
        else {
            ret = cfgls_load_config_cache_hit_flow(&io, json_get_str(in, "path_key", ""), cs);
            json_free(cs);
        }
    } else if (strcmp(func, "last_known_good_fallback") == 0) {
        json_t *cs = cfgls_sig_tuple(json_obj_get(in, "cache_sig"), true);
        if (cs && cfgls_raised(cs)) ret = cs;
        else {
            ret = cfgls_last_known_good_fallback_flow(&io, json_get_str(in, "config_path", ""),
                                                      json_get_str(in, "path_key", ""),
                                                      cs, json_obj_get(in, "exc"));
            json_free(cs);
        }
    } else if (strcmp(func, "merge_managed_overlay") == 0) {
        ret = cfgls_merge_managed_overlay_flow(&io, json_obj_get(in, "expanded"));
    } else if (strcmp(func, "write_config_state") == 0) {
        ret = cfgls_write_config_state_flow(&io, json_get_str(in, "config_path", ""),
                                            json_obj_get(in, "data"),
                                            json_get_bool(in, "allow_omissions", false),
                                            json_get_str(in, "extra_content_on_create", NULL));
    } else if (strcmp(func, "atomic_config_replace") == 0) {
        ret = cfgls_atomic_config_replace(&io, json_get_str(in, "config_path", ""),
                                          json_obj_get(in, "data"),
                                          json_get_str(in, "extra_content_on_create", NULL));
    } else if (strcmp(func, "strip_managed_keys_for_save") == 0) {
        ret = cfgls_strip_managed_keys_for_save(&io, json_obj_get(in, "config"));
    } else if (strcmp(func, "seed_config_file") == 0) {
        ret = cfgls_seed_config_file(&io, json_get_str(in, "config_path", ""),
                                     json_get_str(in, "template", NULL));
    } else if (strcmp(func, "env_ref_lookup") == 0) {
        ret = cfgls_env_ref_lookup(&io, json_get_str(in, "name", ""));
    } else if (strcmp(func, "expand_env_vars") == 0) {
        ret = cfgls_expand_env_vars_flow(&io, json_obj_get(in, "obj"));
    } else if (strcmp(func, "validate_config_key") == 0) {
        ret = cfgls_validate_config_key(&io, json_get_str(in, "key", ""));
    } else if (strcmp(func, "validate_schema_key") == 0) {
        ret = cfgls_validate_schema_key_flow(&io, json_get_str(in, "key", ""));
    } else if (strcmp(func, "is_wrong_prefix_suggestion") == 0) {
        ret = cfgls_is_wrong_prefix_suggestion(&io, json_get_str(in, "key", ""),
                                               json_get_str(in, "suggestion", NULL));
    } else if (strcmp(func, "coerce_config_set_value") == 0) {
        ret = cfgls_coerce_config_set_value_flow(&io, json_get_str(in, "key", ""),
                                                 json_get_str(in, "value", ""));
    } else {
        ret = json_object();
        json_set(ret, "error", json_string("unknown func"));
    }

    json_t *out = json_object();
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", json_copy(io.trace));
    json_set(out, "state", cfgls_state_proj(state, in));
    json_set(out, "stderr", json_copy(io.captured_stderr));
    json_free(io.trace);
    json_free(io.captured_stderr);
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
        json_t *out = cfgls_run_case(func, in);
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
