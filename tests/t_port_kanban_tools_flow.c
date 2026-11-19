/* t_port_kanban_tools_flow.c — harness for the tools/kanban_tools.py FLOW port.
 *
 * Reads JSON LINES from stdin: {"func": <live python name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out": {"ret","trace","state"}}.
 * "trace" is the ordered structured seam-call sequence ({"call": name, ...});
 * "ret" is the return value or a {"__raise__","message"} exception sentinel
 * (message == str(exc); KeyError messages carry repr(key)).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is
 * {"ret": <value>, "raise": {"type","message"}, "state": {...}} (keys
 * optional); a LIST of results is consumed in call order (a queue), a
 * single object is reused for every call. A missing entry returns None.
 * Dict-valued returns must be wrapped in {"ret": {...}}. os.environ is
 * modeled by in.state.env; in.state.delegation_import_fail models the
 * agent.delegation_context import failing inside _delegation_ctx.
 * The oracle drives the LIVE defs under the SAME scripts and compares
 * trace + ret + state.
 */
#include "hermes_json.h"
#include "tools/kanban_tools_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    json_t *io;
    json_t *cursors;
} ktf_script_ctx_t;

static json_t *ktf_script_next(ktf_script_ctx_t *sc, const char *name) {
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

static void ktf_apply_state_effects(json_t *state, json_t *result) {
    if (!result || result->type != JSON_OBJECT) return;
    json_t *eff = json_obj_get(result, "state");
    if (!eff || eff->type != JSON_OBJECT) return;
    for (size_t i = 0; i < eff->c.count; i++)
        json_set(state, eff->c.keys[i], json_copy(eff->c.items[i]));
}

static json_t *ktf_dispatch(ktf_io_t *io, const char *name, json_t *args) {
    (void)args;
    ktf_script_ctx_t *sc = (ktf_script_ctx_t *)io->ctx;
    json_t *result = ktf_script_next(sc, name);
    ktf_apply_state_effects(io->state, result);
    if (result && result->type == JSON_OBJECT) {
        json_t *raise = json_obj_get(result, "raise");
        if (raise && raise->type == JSON_OBJECT) {
            const char *t = json_get_str(raise, "type", "Exception");
            const char *m = json_get_str(raise, "message", "");
            json_t *exc = json_object();
            json_set(exc, "__raise__", json_string(t));
            /* str(KeyError(k)) == repr(k): normalize at spec creation so
             * every raise spec's message equals str(exc). */
            if (strcmp(t, "KeyError") == 0) {
                char *r = ktf_repr_str(m);
                json_set(exc, "message", json_string(r ? r : ""));
                free(r);
            } else {
                json_set(exc, "message", json_string(m));
            }
            return exc;
        }
        json_t *ret = json_obj_get(result, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return result ? json_copy(result) : json_null();
}

static void ktf_io_init(ktf_io_t *io, ktf_script_ctx_t *sc, json_t *in,
                        json_t *state) {
    io->call = ktf_dispatch;
    io->trace = json_array();
    io->state = state;
    io->ctx = sc;
    sc->io = json_obj_get(in, "io");
    sc->cursors = json_object();
}

static json_t *ktf_state_from(json_t *in) {
    json_t *s = json_obj_get(in, "state");
    return (s && s->type == JSON_OBJECT) ? json_copy(s) : json_object();
}

static json_t *ktf_make_out(ktf_io_t *io, json_t *ret) {
    json_t *out = json_object();
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", io->trace ? json_copy(io->trace) : json_array());
    json_set(out, "state", io->state ? json_copy(io->state) : json_object());
    return out;
}

static json_t *ktf_run_case(const char *func, json_t *in) {
    ktf_io_t io;
    ktf_script_ctx_t sc;
    json_t *state = ktf_state_from(in);
    ktf_io_init(&io, &sc, in, state);
    json_t *args = json_obj_get(in, "args");
    json_t *ret = NULL;

    if (strcmp(func, "_delegation_ctx") == 0) {
        ret = ktf_delegation_ctx(&io, json_get_str(in, "predicate", ""),
                                 json_get_bool(in, "default", false));
    } else if (strcmp(func, "_is_dispatcher_owned_worker") == 0) {
        ret = ktf_is_dispatcher_owned_worker(&io);
    } else if (strcmp(func, "_visible") == 0) {
        ret = ktf_visible(&io, json_get_bool(in, "to_env_worker", false));
    } else if (strcmp(func, "_check") == 0) {
        ret = ktf_check(&io, json_obj_get(in, "cond"),
                        json_get_str(in, "message", ""));
    } else if (strcmp(func, "_require_text") == 0) {
        ret = ktf_require_text(&io, args, json_get_str(in, "name", ""),
                               json_obj_get(in, "message") &&
                                       json_obj_get(in, "message")->type == JSON_STRING
                                   ? json_obj_get(in, "message")->str_val
                                   : NULL);
    } else if (strcmp(func, "_coerce_str_list") == 0) {
        ret = ktf_coerce_str_list(&io, json_obj_get(in, "value"),
                                  json_get_str(in, "name", ""),
                                  json_get_str(in, "what", ""),
                                  json_get_bool(in, "strip", false));
    } else if (strcmp(func, "_require_dict_metadata") == 0) {
        ret = ktf_require_dict_metadata(&io, json_obj_get(in, "metadata"));
    } else if (strcmp(func, "_merge_artifacts") == 0) {
        ret = ktf_merge_artifacts(&io, json_obj_get(in, "metadata"),
                                  json_obj_get(in, "artifacts"));
    } else if (strcmp(func, "_opt_int") == 0) {
        ret = ktf_opt_int(json_obj_get(in, "value"), json_obj_get(in, "default"));
    } else if (strcmp(func, "_fields") == 0) {
        ret = ktf_fields(json_obj_get(in, "obj"), json_obj_get(in, "names"));
    } else if (strcmp(func, "_redact") == 0) {
        ret = ktf_redact(&io, json_obj_get(in, "value"));
    } else if (strcmp(func, "_redact_opt") == 0) {
        ret = ktf_redact_opt(&io, json_obj_get(in, "value"));
    } else if (strcmp(func, "_own_task_env") == 0) {
        ret = ktf_own_task_env(&io, json_get_str(in, "task_id", ""),
                               json_get_str(in, "var", ""));
    } else if (strcmp(func, "_reject_inherited_task_mutation") == 0) {
        ret = ktf_reject_inherited_task_mutation(&io,
            json_get_str(in, "tid", ""), json_obj_get(in, "env_tid"));
    } else if (strcmp(func, "_require_task_id") == 0) {
        ret = ktf_require_task_id(&io, args);
    } else if (strcmp(func, "_worker_guard") == 0) {
        ret = ktf_worker_guard(&io, json_get_str(in, "tool_name", ""), args);
    } else if (strcmp(func, "_kanban_handler") == 0) {
        ret = ktf_kanban_handler(&io, json_get_str(in, "tool_name", ""), args);
    } else if (strcmp(func, "_ok_landed") == 0) {
        ret = ktf_ok_landed(&io, json_get_str(in, "task_id", ""),
                            json_get_str(in, "default_status", ""),
                            json_obj_get(in, "extra"));
    } else {
        ret = json_object();
        json_set(ret, "error", json_string("unknown func"));
    }

    json_t *out = ktf_make_out(&io, ret);
    json_free(sc.cursors);
    json_free(io.trace);
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
        json_t *out = ktf_run_case(func, in);
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
