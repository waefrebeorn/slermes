/* t_port_kanban_db_flow.c — harness for the hermes_cli/kanban_db.py FLOW port.
 *
 * Reads JSON LINES from stdin: {"func": <live python name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out": {"ret","trace","state"}}.
 * "trace" is the ordered structured seam-call sequence ({"call": name, ...});
 * "ret" is the return value or a {"__raise__","message"} exception sentinel.
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is
 * {"ret": <value>, "state": {...}, "raise": {"type","message"}} (keys
 * optional). A result may be a LIST consumed in call order (a queue); a
 * single object is reused for every call. For the "execute" seam, "ret" is a
 * cursor spec {"rowcount","lastrowid","rows","fetch_raise"}; fetchone/fetchall
 * consume rows in order and record their own trace events. The oracle drives
 * the LIVE defs under the SAME scripts and compares trace + ret + state.
 */
#include "hermes_json.h"
#include "port_kanban_db_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    json_t *io;
    json_t *cursors;
} script_ctx_t;

static json_t *script_next(script_ctx_t *sc, const char *name) {
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

static void apply_state_effects(json_t *state, json_t *result) {
    if (!result || result->type != JSON_OBJECT) return;
    json_t *eff = json_obj_get(result, "state");
    if (!eff || eff->type != JSON_OBJECT) return;
    for (size_t i = 0; i < eff->c.count; i++)
        json_set(state, eff->c.keys[i], json_copy(eff->c.items[i]));
}

static json_t *dispatch(kdbf_io_t *io, const char *name, json_t *args) {
    (void)args;
    script_ctx_t *sc = (script_ctx_t *)io->ctx;
    json_t *result = script_next(sc, name);
    apply_state_effects(io->state, result);
    if (result && result->type == JSON_OBJECT) {
        json_t *raise = json_obj_get(result, "raise");
        if (raise && raise->type == JSON_OBJECT) {
            json_t *exc = json_object();
            json_set(exc, "__raise__", json_copy(json_obj_get(raise, "type")));
            json_t *msg = json_obj_get(raise, "message");
            json_set(exc, "message", msg ? json_copy(msg) : json_string(""));
            return exc;
        }
        json_t *ret = json_obj_get(result, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return json_null();
}

static void io_init(kdbf_io_t *io, script_ctx_t *sc, json_t *in, json_t *state) {
    io->call = dispatch;
    io->trace = json_array();
    io->state = state;
    io->ctx = sc;
    sc->io = json_obj_get(in, "io");
    sc->cursors = json_object();
}

static json_t *state_from(json_t *in) {
    json_t *s = json_obj_get(in, "state");
    return (s && s->type == JSON_OBJECT) ? json_copy(s) : json_object();
}

static json_t *make_out(kdbf_io_t *io, json_t *ret) {
    json_t *out = json_object();
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", io->trace ? json_copy(io->trace) : json_array());
    json_set(out, "state", io->state ? json_copy(io->state) : json_object());
    return out;
}

static json_t *run_case(const char *func, json_t *in) {
    kdbf_io_t io;
    script_ctx_t sc;
    json_t *state = state_from(in);
    io_init(&io, &sc, in, state);
    const char *tid = json_get_str(in, "task_id", "");
    json_t *ret = NULL;

    if (strcmp(func, "_task_status") == 0) {
        ret = kdbf_task_status(&io, tid);
    } else if (strcmp(func, "_current_run_id") == 0) {
        ret = kdbf_current_run_id(&io, tid);
    } else if (strcmp(func, "_extend_run_claim") == 0) {
        ret = kdbf_extend_run_claim(&io, tid, json_get_num(in, "expires", 0));
    } else if (strcmp(func, "unsatisfied_parents") == 0) {
        ret = kdbf_unsatisfied_parents(&io, tid);
    } else if (strcmp(func, "_missing_task_ids") == 0) {
        ret = kdbf_missing_task_ids(&io, json_obj_get(in, "ids"));
    } else if (strcmp(func, "_prior_reviewer") == 0) {
        ret = kdbf_prior_reviewer(&io, tid);
    } else if (strcmp(func, "_claim_is_live") == 0) {
        ret = kdbf_claim_is_live(&io, json_obj_get(in, "trow"));
    } else if (strcmp(func, "_validate_model_override") == 0) {
        ret = kdbf_validate_model_override(json_obj_get(in, "model"), json_obj_get(in, "provider"));
    } else if (strcmp(func, "_normalize_task_skills") == 0) {
        ret = kdbf_normalize_task_skills(&io, json_obj_get(in, "skills"));
    } else if (strcmp(func, "_completed_event_payload") == 0) {
        ret = kdbf_completed_event_payload(json_obj_get(in, "result"),
            json_obj_get(in, "event_summary"), json_obj_get(in, "verified_cards"),
            json_obj_get(in, "metadata"));
    } else if (strcmp(func, "_route_block") == 0) {
        ret = kdbf_route_block_flow(json_obj_get(in, "kind"), json_obj_get(in, "reason"),
            json_get_str(in, "source_status", ""), json_obj_get(in, "prev_kind"),
            json_get_num(in, "prev_recurrences", 0));
    } else if (strcmp(func, "_insert_comment") == 0) {
        ret = kdbf_insert_comment(&io, tid, json_get_str(in, "author", ""),
            json_get_str(in, "body", ""), json_get_num(in, "created_at", 0));
    } else if (strcmp(func, "_delete_task_relations") == 0) {
        ret = kdbf_delete_task_relations_flow(&io, tid);
    } else if (strcmp(func, "_record_reclaim") == 0) {
        ret = kdbf_record_reclaim_flow(&io, tid, json_obj_get(in, "termination"),
            json_get_str(in, "error", ""), json_obj_get(in, "payload"));
    } else if (strcmp(func, "_set_task_override") == 0) {
        ret = kdbf_set_task_override_flow(&io, tid, json_get_str(in, "sql", ""),
            json_obj_get(in, "params"), json_get_str(in, "event_kind", ""),
            json_obj_get(in, "payload"), json_obj_get(in, "changed_fields"),
            json_get_str(in, "archived_msg", ""));
    } else if (strcmp(func, "_gate_empty_completion") == 0) {
        ret = kdbf_gate_empty_completion_flow(&io, tid, json_obj_get(in, "result"),
            json_obj_get(in, "summary"));
    } else if (strcmp(func, "_extend_live_stale_claim") == 0) {
        ret = kdbf_extend_live_stale_claim_flow(&io, json_obj_get(in, "row"),
            json_get_num(in, "now", 0));
    } else if (strcmp(func, "_claim_and_open_run") == 0) {
        ret = kdbf_claim_and_open_run_flow(&io, tid, json_get_str(in, "source_status", ""),
            json_get_str(in, "lock", ""), json_get_num(in, "expires", 0),
            json_get_num(in, "now", 0), json_obj_get(in, "event_extra"),
            json_obj_get(in, "expect_assignee"));
    } else if (strcmp(func, "edit_task") == 0) {
        ret = kdbf_edit_task_flow(&io, tid, json_obj_get(in, "title"),
            json_obj_get(in, "body"), json_obj_get(in, "priority"),
            json_obj_get(in, "result"), json_obj_get(in, "summary"),
            json_obj_get(in, "metadata"), json_obj_get(in, "board"));
    } else {
        ret = json_object();
        json_set(ret, "error", json_string("unknown func"));
    }

    json_t *out = make_out(&io, ret);
    json_free(sc.cursors);
    json_free(io.trace);
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
        json_t *out = run_case(func, in);
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
