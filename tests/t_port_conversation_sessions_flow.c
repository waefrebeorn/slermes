/* t_port_conversation_sessions_flow.c — harness for the
 * persistence/conversation/sessions.py FLOW + leaves port
 * (src/persistence/conversation_sessions_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <live_name>, "in": {...}}.
 * Emits JSON LINES: {"func","in","out": {"ret","trace","state"}}.
 * "trace" is the ordered seam-event sequence ({"call":name,...args}); "state"
 * carries the mutated self for RecentBrowseQuery.__post_init__ (else {}).
 *
 * Scripted I/O lives in in.io (seam-name -> {"ret":v} / {"raise":spec} or a
 * per-call queue LIST of those). The dispatcher applies it exactly as the
 * oracle's recording stubs are fed. Missing/exhausted scripts FAIL CLOSED with
 * a RuntimeError "no script for <name>" raise (recorded, then propagated).
 * The harness echoes "in" with json_copy (json_set takes ownership).
 */
#include "libjson/json.h"
#include "persistence/conversation_sessions_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct { json_t *io; json_t *cursors; } cs_script_ctx_t;

static json_t *cs_script_next(cs_script_ctx_t *sc, const char *name) {
    if (!sc || !sc->io) return NULL;
    json_t *entry = json_obj_get(sc->io, name);
    if (!entry) return NULL;
    if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) return NULL;   /* exhaustion fails closed */
        json_set(sc->cursors, name, json_number(cur + 1));
        return json_get(entry, idx);
    }
    return entry;
}

static json_t *cs_raise_from_spec(const json_t *spec) {
    const char *t = json_get_str(spec, "type", "Exception");
    const char *m = json_get_str(spec, "message", "");
    return cs_make_raise(t, m);
}

static json_t *cs_dispatch(cs_io_t *io, const char *name, json_t *args) {
    (void)args;
    cs_script_ctx_t *sc = (cs_script_ctx_t *)io->ctx;
    json_t *result = cs_script_next(sc, name);
    if (!result) {
        char *msg = cs_asprintf("no script for %s", name);
        json_t *e = cs_make_raise("RuntimeError", msg ? msg : "");
        free(msg);
        return e;
    }
    if (result->type == JSON_OBJECT) {
        json_t *raise = json_obj_get(result, "raise");
        if (raise && raise->type == JSON_OBJECT) return cs_raise_from_spec(raise);
        json_t *ret = json_obj_get(result, "ret");
        if (ret) return json_copy(ret);
    }
    return json_copy(result);
}

static json_t *cs_run_case(const char *func, json_t *in) {
    cs_io_t io;
    cs_script_ctx_t sc;
    io.call = cs_dispatch;
    io.trace = json_array();
    sc.io = json_obj_get(in, "io");
    sc.cursors = json_object();
    io.ctx = &sc;
    json_t *ret = NULL;
    json_t *state = json_object();
    json_t *self = json_obj_get(in, "self");

    if (strcmp(func, "is_automatic_end_reason") == 0) {
        ret = cs_is_automatic_end_reason(json_obj_get(in, "reason"));
    } else if (strcmp(func, "is_explicit_close_reason") == 0) {
        ret = cs_is_explicit_close_reason(&io, json_obj_get(in, "reason"));
    } else if (strcmp(func, "bound_activity_description") == 0) {
        ret = cs_bound_activity_description(json_obj_get(in, "description"));
    } else if (strcmp(func, "normalize_activity_provenance") == 0) {
        ret = cs_normalize_activity_provenance(json_obj_get(in, "provenance"));
    } else if (strcmp(func, "workspace_key") == 0) {
        ret = cs_workspace_key(json_obj_get(in, "row"));
    } else if (strcmp(func, "parse_model_config") == 0) {
        ret = cs_parse_model_config(&io, json_obj_get(in, "raw"));
    } else if (strcmp(func, "is_corrupt_model_config") == 0) {
        ret = cs_is_corrupt_model_config(&io, json_obj_get(in, "raw"));
    } else if (strcmp(func, "session_yolo_enabled") == 0) {
        ret = cs_session_yolo_enabled(&io, json_obj_get(in, "model_config"));
    } else if (strcmp(func, "sanitize_title") == 0) {
        ret = cs_sanitize_title(json_obj_get(in, "title"));
    } else if (strcmp(func, "TitleConflict.__init__") == 0) {
        ret = cs_title_conflict_init(json_obj_get(in, "title"), json_obj_get(in, "holder"));
    } else if (strcmp(func, "CanonicalTitleLocked.__init__") == 0) {
        ret = cs_canonical_title_locked_init(json_obj_get(in, "session_id"));
    } else if (strcmp(func, "SessionExists.__init__") == 0) {
        ret = cs_session_exists_init(json_obj_get(in, "existing"));
    } else if (strcmp(func, "NewSession.__post_init__") == 0) {
        ret = cs_new_session_post_init(self);
    } else if (strcmp(func, "SessionRecord._row_cells") == 0) {
        ret = cs_session_record_row_cells(self, json_get_bool(in, "compact", false));
    } else if (strcmp(func, "SessionRecord._marker_cells") == 0) {
        ret = cs_session_record_marker_cells(self);
    } else if (strcmp(func, "SessionRecord.as_session_row") == 0) {
        ret = cs_session_record_as_session_row(&io, self);
    } else if (strcmp(func, "SessionWindow.__post_init__") == 0) {
        ret = cs_session_window_post_init(self);
    } else if (strcmp(func, "SessionListEntry._head_cells") == 0) {
        ret = cs_session_list_entry_head_cells(&io, self, json_obj_get(in, "extra"));
    } else if (strcmp(func, "SessionListEntry.as_list_row") == 0) {
        ret = cs_session_list_entry_as_list_row(&io, self);
    } else if (strcmp(func, "CronRunEntry.as_cron_row") == 0) {
        ret = cs_cron_run_entry_as_cron_row(&io, self);
    } else if (strcmp(func, "RecentBrowseQuery.__post_init__") == 0) {
        json_t *slf = self ? json_copy(self) : json_object();
        ret = cs_recent_browse_query_post_init(slf);
        json_set(state, "self", slf);   /* mutated (or partially mutated) self */
    } else if (strcmp(func, "RuntimeLock.as_mapping") == 0) {
        ret = cs_runtime_lock_as_mapping(self);
    } else if (strcmp(func, "ReplaceSessionConfig.__post_init__") == 0) {
        ret = cs_replace_session_config_post_init(self);
    } else {
        ret = json_object();
        json_set(ret, "error", json_string("unknown func"));
    }

    json_t *out = json_object();
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", json_copy(io.trace));
    json_set(out, "state", state);
    json_free(io.trace);
    json_free(sc.cursors);
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
        json_t *out = cs_run_case(func, in);
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