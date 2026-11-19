/* t_port_cron_jobs_flow.c — harness for the cron/jobs.py FLOW port.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out": {"ret","trace","state"[,...]}}.
 * "trace" is the ordered seam-call NAME sequence; "state" the mutated module
 * globals. Scripted I/O lives in in.io: seam-name -> result, where a result
 * is {"ret": <value>, "state": {<effects>}, "raise": {"type","message"}}
 * (keys optional). A LIST result is a per-call queue; a single object is
 * reused for every call. List-valued RETURNS must be wrapped {"ret": [...]}
 * (a bare array is a queue). Mirrors tests/oracle_cron_jobs_flow.py's
 * recording stubs fed the same scripts.
 */
#include "hermes_json.h"
#include "cron/cron_jobs_flow.c"

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

static json_t *dispatch(cjf_io_t *io, const char *name, json_t *args) {
    (void)args;  /* name-only trace (scheduler_flow convention) */
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
            json_t *en = json_obj_get(raise, "errno");
            if (en) json_set(exc, "errno", json_copy(en));
            return exc;
        }
        json_t *ret = json_obj_get(result, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return json_null();
}

static void io_init(cjf_io_t *io, script_ctx_t *sc, json_t *in, json_t *state) {
    io->call = dispatch;
    io->trace = json_array();
    io->state = state;
    io->ctx = sc;
    sc->io = json_obj_get(in, "io");
    sc->cursors = json_object();
    /* live module-global init at import: _timezone_migration_catchups_recent
     * exists (empty list) even when no catchup ever ran — mirror it so the
     * state snapshot matches the oracle's unconditional seed. */
    if (state && !json_obj_get(state, "_timezone_migration_catchups_recent"))
        json_set(state, "_timezone_migration_catchups_recent", json_array());
}

static json_t *state_from(json_t *in) {
    json_t *s = json_obj_get(in, "state");
    return (s && s->type == JSON_OBJECT) ? json_copy(s) : json_object();
}

static json_t *make_out(cjf_io_t *io, json_t *ret) {
    json_t *out = json_object();
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", io->trace ? json_copy(io->trace) : json_array());
    json_set(out, "state", io->state ? json_copy(io->state) : json_object());
    return out;
}

static json_t *ensure_recent(json_t *state) {
    json_t *r = json_obj_get(state, "recent");
    if (!r || r->type != JSON_ARRAY) {
        r = json_array();
        json_set(state, "recent", r);
    }
    return r;
}

static json_t *run_case(const char *func, json_t *in) {
    cjf_io_t io;
    script_ctx_t sc;
    json_t *state = state_from(in);
    io_init(&io, &sc, in, state);
    json_t *ret = NULL;
    json_t *updates = NULL;
    json_t *claim = NULL;
    json_t *jobs_echo = NULL;

    if (strcmp(func, "self_removal_delivery_allowed") == 0) {
        ret = cjf_self_removal_delivery_allowed(&io, json_get_str(in, "job_id", ""));
    } else if (strcmp(func, "acquire_flock") == 0) {
        ret = cjf_acquire_flock(&io, json_get_num(in, "lock_fd", 0),
                                json_get_num(in, "timeout", 30.0));
    } else if (strcmp(func, "release_flock") == 0) {
        ret = cjf_release_flock(&io, json_get_num(in, "lock_fd", 0));
    } else if (strcmp(func, "ensure_cron_dir") == 0) {
        ret = cjf_ensure_cron_dir(&io, json_get_str(in, "cron_dir", ""));
    } else if (strcmp(func, "cron_schedule") == 0) {
        ret = cjf_cron_schedule(&io, json_get_str(in, "expr", ""),
                                json_get_str(in, "display", ""),
                                json_get_str(in, "missing_croniter", ""),
                                json_get_str(in, "invalid_label", ""));
    } else if (strcmp(func, "parse_aware") == 0) {
        ret = cjf_parse_aware(&io, json_obj_get(in, "value"), json_obj_get(in, "zone"));
    } else if (strcmp(func, "append_telemetry_record") == 0) {
        json_t *recent = ensure_recent(state);
        ret = cjf_append_telemetry_record(&io, json_get_str(in, "filename", "t.jsonl"),
                                          json_obj_get(in, "entry"), recent);
    } else if (strcmp(func, "record_timezone_migration_catchup") == 0) {
        ret = cjf_record_timezone_migration_catchup(&io, json_obj_get(in, "job"),
                                                    json_get_str(in, "raw_next_run_at", ""),
                                                    json_get_str(in, "normalized_next_run_at", ""));
    } else if (strcmp(func, "write_marker") == 0) {
        ret = cjf_write_marker(&io, json_get_str(in, "name", ""),
                               json_get_str(in, "text", ""),
                               json_get_str(in, "tmp_prefix", ""));
    } else if (strcmp(func, "read_marker_fields") == 0) {
        ret = cjf_read_marker_fields(&io, json_get_str(in, "name", ""));
    } else if (strcmp(func, "ticker_heartbeat_writer_alive") == 0) {
        ret = cjf_ticker_heartbeat_writer_alive(&io);
    } else if (strcmp(func, "unmerged_disk_jobs") == 0) {
        ret = cjf_unmerged_disk_jobs(&io, json_obj_get(in, "jobs"),
                                     json_obj_get(in, "removed_ids"));
    } else if (strcmp(func, "unlink_quiet") == 0) {
        ret = cjf_unlink_quiet(&io, json_obj_get(in, "path"));
    } else if (strcmp(func, "stage_jobs_payload") == 0) {
        ret = cjf_stage_jobs_payload(&io, json_get_str(in, "jobs_file", ""),
                                     json_obj_get(in, "jobs"));
    } else if (strcmp(func, "with_job") == 0) {
        ret = cjf_with_job(&io, json_obj_get(in, "job_id"), json_obj_get(in, "missing"));
    } else if (strcmp(func, "next_run_or_reject_past_oneshot") == 0) {
        ret = cjf_next_run_or_reject_past_oneshot(&io, json_obj_get(in, "parsed_schedule"),
                                                  json_get_str(in, "label", ""),
                                                  json_obj_get(in, "fallback_run_at"),
                                                  json_get_str(in, "what", ""),
                                                  json_obj_get(in, "timezone"));
    } else if (strcmp(func, "apply_pin_update") == 0) {
        updates = json_copy(json_obj_get(in, "updates"));
        if (!updates) updates = json_object();
        ret = cjf_apply_pin_update(&io, json_obj_get(in, "job"), updates);
    } else if (strcmp(func, "refresh_claim") == 0) {
        claim = json_copy(json_obj_get(in, "claim"));
        jobs_echo = json_copy(json_obj_get(in, "jobs"));
        ret = cjf_refresh_claim(&io, jobs_echo, claim, json_get_str(in, "expected_owner", ""));
    } else {
        ret = json_object();
        json_set(ret, "error", json_string("unknown func"));
    }

    json_t *out = make_out(&io, ret);
    if (updates) json_set(out, "updates", updates);
    if (claim) json_set(out, "claim", claim);
    if (jobs_echo) json_set(out, "jobs", jobs_echo);
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