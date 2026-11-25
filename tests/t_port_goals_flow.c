/* t_port_goals_flow.c — harness for the UNCLAIMED goal FLOW port
 * (src/cli/goals_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func","in","out":{"ret","trace","state","saves"}}.
 * "trace" is the ordered seam-name sequence; "state" the mutated caller-held
 * mirror (restricted to the keys the case seeded); "saves" the captured
 * save_goal payloads ({session_id, state} per call, computed from seam args).
 *
 * Scripted I/O lives in in.io (seam-name -> {"ret","raise","import_raise"}
 * or a queue LIST). EVERY seam is FAIL-CLOSED: a missing/exhausted script
 * raises RuntimeError("unscripted seam: <name>"). An "import_raise" entry
 * models a failed in-function import (no trace token: __untrace__).
 *
 * State materialization: in.state.manager._state and in.state.goal_state
 * are default-filled GoalState dicts (gl_mat_state), mirroring the oracle's
 * mat_goal_state so partial fixture dicts agree on both sides.
 */
#include "hermes_json.h"
#include "cli/goals_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    json_t *io;
    json_t *cursors;
} gl_script_ctx_t;

static json_t *gl_script_next(gl_script_ctx_t *sc, const char *name) {
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

static json_t *gl_dispatch(gl_flow_io_t *io, const char *name, json_t *args) {
    gl_script_ctx_t *sc = (gl_script_ctx_t *)io->ctx;
    json_t *result = gl_script_next(sc, name);
    if (!result) {
        char *m = gl_join("unscripted seam: ", name);
        json_t *r = gl_make_raise("RuntimeError", m);
        free(m);
        return r;
    }
    /* save_goal captures the persisted payload from the seam ARGS. */
    if (strcmp(name, "save_goal") == 0 && io->cap_saves && io->cap_saves->type == JSON_ARRAY) {
        json_t *row = json_object();
        json_t *sid = args ? json_obj_get(args, "session_id") : NULL;
        json_t *st = args ? json_obj_get(args, "state") : NULL;
        json_set(row, "session_id", sid ? json_copy(sid) : json_null());
        json_set(row, "state", st ? json_copy(st) : json_null());
        json_append(io->cap_saves, row);
    }
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

static json_t *gl_state_from(json_t *in) {
    json_t *s = json_obj_get(in, "state");
    json_t *out = (s && s->type == JSON_OBJECT) ? json_copy(s) : json_object();
    json_t *m = json_obj_get(out, "manager");
    if (m && m->type == JSON_OBJECT)
        json_set(m, "_state", gl_mat_state(json_obj_get(m, "_state")));
    json_t *gs = json_obj_get(out, "goal_state");
    if (gs) json_set(out, "goal_state", gl_mat_state(gs));
    return out;
}

static json_t *gl_state_proj(json_t *state, json_t *in) {
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

static void gl_io_init(gl_flow_io_t *io, gl_script_ctx_t *sc, json_t *in, json_t *state) {
    io->call = gl_dispatch;
    io->trace = json_array();
    io->state = state;
    io->cap_saves = json_array();
    io->ctx = sc;
    sc->io = json_obj_get(in, "io");
    sc->cursors = json_object();
}

static json_t *gl_run_case(const char *func, json_t *in) {
    gl_flow_io_t io;
    gl_script_ctx_t sc;
    json_t *state = gl_state_from(in);
    gl_io_init(&io, &sc, in, state);
    json_t *ret = NULL;

    if (strcmp(func, "decision") == 0) {
        ret = gl_decision(json_obj_get(in, "status"), json_get_bool(in, "should_continue", false),
                          json_obj_get(in, "prompt"), json_get_str(in, "verdict", ""),
                          json_get_str(in, "reason", ""), json_get_str(in, "message", ""));
    } else if (strcmp(func, "render_extra_criteria") == 0) {
        ret = gl_render_extra_criteria(json_obj_get(in, "subgoals"));
    } else if (strcmp(func, "goal_kick_prompt") == 0) {
        ret = gl_goal_kick_prompt(json_obj_get(in, "goal"), json_obj_get(in, "last_user_message"));
    } else if (strcmp(func, "last_user_message_content") == 0) {
        ret = gl_last_user_message_content(json_obj_get(in, "history"));
    } else if (strcmp(func, "count_active_delegations") == 0) {
        ret = gl_count_active_delegations(&io, json_obj_get(in, "session_id"));
    } else if (strcmp(func, "last_user_message_from_db") == 0) {
        ret = gl_last_user_message_from_db(&io, json_obj_get(in, "session_id"));
    } else if (strcmp(func, "goal_judge_setting") == 0) {
        ret = gl_goal_judge_setting(&io, json_obj_get(in, "key"), json_obj_get(in, "default"),
                                    json_get_str(in, "cast", "int"));
    } else if (strcmp(func, "call_goal_judge_llm") == 0) {
        ret = gl_call_goal_judge_llm(&io, json_obj_get(in, "system_prompt"),
                                     json_obj_get(in, "user_prompt"), json_obj_get(in, "timeout"));
    } else if (strcmp(func, "gate_workspace") == 0) {
        ret = gl_gate_workspace(&io);
    } else if (strcmp(func, "acquire_session_db") == 0) {
        ret = gl_acquire_session_db(&io, json_obj_get(in, "home"));
    } else if (strcmp(func, "release_session_db") == 0) {
        ret = gl_release_session_db(&io, json_obj_get(in, "db"));
    } else if (strcmp(func, "clear_wait") == 0) {
        ret = gl_clear_wait(json_obj_get(state, "goal_state"));
    } else if (strcmp(func, "refresh") == 0) {
        ret = gl_manager_refresh(&io);
    } else if (strcmp(func, "save") == 0) {
        ret = gl_manager_save(&io);
    } else if (strcmp(func, "require_goal") == 0) {
        ret = gl_manager_require_goal(&io);
    } else if (strcmp(func, "require_active") == 0) {
        ret = gl_manager_require_active(&io);
    } else if (strcmp(func, "pause_state") == 0) {
        ret = gl_manager_pause_state(&io, json_obj_get(in, "reason"));
    } else if (strcmp(func, "pause_decision") == 0) {
        ret = gl_manager_pause_decision(&io, json_get_str(in, "paused_reason", ""),
                                        json_get_str(in, "verdict", ""), json_get_str(in, "reason", ""),
                                        json_get_str(in, "message", ""));
    } else if (strcmp(func, "pop_item") == 0) {
        ret = gl_manager_pop_item(&io, json_get_str(in, "attr", "subgoals"), json_obj_get(in, "index"));
    } else if (strcmp(func, "clear_items") == 0) {
        ret = gl_manager_clear_items(&io, json_get_str(in, "attr", "subgoals"));
    } else if (strcmp(func, "park") == 0) {
        ret = gl_manager_park(&io, json_obj_get(in, "reason"), json_obj_get(in, "barrier"));
    } else if (strcmp(func, "waiting_decision") == 0) {
        ret = gl_manager_waiting_decision(&io, gl_mgr_state(&io));
    } else if (strcmp(func, "apply_wait_directive") == 0) {
        ret = gl_manager_apply_wait_directive(&io, json_obj_get(in, "directive"),
                                              json_obj_get(in, "reason"),
                                              json_obj_get(in, "active_delegations"));
    } else if (strcmp(func, "budget_pause") == 0) {
        ret = gl_manager_budget_pause(&io, gl_mgr_state(&io), json_get_str(in, "verdict", ""),
                                      json_get_str(in, "reason", ""), json_obj_get(in, "note"));
    } else {
        ret = json_object();
        json_set(ret, "error", json_string("unknown func"));
    }

    json_t *out = json_object();
    if (gl_raised(ret)) {
        json_t *spec = json_copy(gl_raise_spec(ret));
        json_free(ret);
        ret = json_object();
        json_set(ret, "__raise__", spec);
    }
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", json_copy(io.trace));
    json_set(out, "state", gl_state_proj(state, in));
    json_set(out, "saves", json_copy(io.cap_saves));
    json_free(io.trace);
    json_free(io.cap_saves);
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
        json_t *out = gl_run_case(func, in);
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