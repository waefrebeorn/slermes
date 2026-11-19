/* t_port_scheduler_flow.c — harness for the cron/scheduler.py FLOW-core port.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out": {"ret","trace","state"[,"scope"]}}.
 * "trace" is the ordered seam-call sequence; "state" the mutated module globals.
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is
 * {"ret": <value>, "state": {<attr effects>}, "raise": {"type","message","errno"}}
 * (keys optional). A result may be a LIST consumed in call order (a queue); a
 * single object is reused for every call. The dispatcher applies "state" effects
 * onto io->state and models "raise" as an exception sentinel, mirroring the
 * oracle's recording stubs fed the same scripts.
 */
#include "hermes_json.h"
#include "port_scheduler_flow.c"

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

static json_t *dispatch(sf_flow_io_t *io, const char *name, json_t *args) {
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
            json_t *en = json_obj_get(raise, "errno");
            if (en) json_set(exc, "errno", json_copy(en));
            json_t *res = json_obj_get(raise, "result");
            if (res) json_set(exc, "result", json_copy(res));
            return exc;
        }
        json_t *ret = json_obj_get(result, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return json_null();
}

static void io_init(sf_flow_io_t *io, script_ctx_t *sc, json_t *in, json_t *state) {
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

static json_t *make_out(sf_flow_io_t *io, json_t *ret, json_t *scope) {
    json_t *out = json_object();
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", io->trace ? json_copy(io->trace) : json_array());
    json_set(out, "state", io->state ? json_copy(io->state) : json_object());
    if (scope) json_set(out, "scope", json_copy(scope));
    return out;
}

static json_t *run_case(const char *func, json_t *in) {
    sf_flow_io_t io;
    script_ctx_t sc;
    json_t *state = state_from(in);
    io_init(&io, &sc, in, state);
    json_t *ret = NULL;
    json_t *scope = NULL;

    if (strcmp(func, "job_route_pinned") == 0) {
        ret = json_bool(sf_job_route_pinned(json_obj_get(in, "job")));
    } else if (strcmp(func, "normalize_newlines") == 0) {
        char *r = sf_normalize_newlines(json_get_str(in, "text", ""));
        ret = json_string(r ? r : ""); free(r);
    } else if (strcmp(func, "cron_failure_marker_error") == 0) {
        char *r = sf_cron_failure_marker_error(json_get_str(in, "text", ""));
        ret = r ? json_string(r) : json_null(); free(r);
    } else if (strcmp(func, "apply_agent_failure_marker") == 0) {
        ret = sf_apply_agent_failure_marker(json_obj_get(in, "job"),
            json_get_bool(in, "success", false), json_obj_get(in, "error"),
            json_get_str(in, "final_response", ""));
    } else if (strcmp(func, "is_lock_contention_errno") == 0) {
        ret = json_bool(sf_is_lock_contention_errno(json_get_str(in, "errno", ""),
            json_get_str(in, "platform", "fcntl")));
    } else if (strcmp(func, "is_fd_exhaustion_text") == 0) {
        ret = json_bool(sf_is_fd_exhaustion_text(json_get_str(in, "text", "")));
    } else if (strcmp(func, "is_fd_exhaustion") == 0) {
        ret = json_bool(sf_is_fd_exhaustion(json_obj_get(in, "exc")));
    } else if (strcmp(func, "stale_code_yield_labels") == 0) {
        ret = sf_stale_code_yield_labels(json_get_str(in, "recorded_error", ""));
    } else if (strcmp(func, "inflight_key") == 0) {
        ret = sf_inflight_key(&io, json_get_str(in, "job_id", ""), json_obj_get(in, "home"));
    } else if (strcmp(func, "is_job_running") == 0) {
        ret = json_bool(sf_is_job_running(&io, json_get_str(in, "job_id", ""), json_obj_get(in, "home")));
    } else if (strcmp(func, "job_interval_minutes") == 0) {
        ret = sf_job_interval_minutes(&io, json_obj_get(in, "job"));
    } else if (strcmp(func, "failure_repeat_alert_hours") == 0) {
        ret = json_number(sf_failure_repeat_alert_hours(&io));
    } else if (strcmp(func, "repeat_alert_withheld") == 0) {
        ret = json_bool(sf_repeat_alert_withheld(&io, json_obj_get(in, "incident")));
    } else if (strcmp(func, "failure_streak_nudge") == 0) {
        char *r = sf_failure_streak_nudge(&io, json_obj_get(in, "job"));
        ret = json_string(r ? r : ""); free(r);
    } else if (strcmp(func, "inflight_min_allowance_minutes") == 0) {
        ret = json_number(sf_inflight_min_allowance_minutes(&io));
    } else if (strcmp(func, "cron_cleanup_timeout_seconds") == 0) {
        ret = json_number(sf_cron_cleanup_timeout_seconds(&io));
    } else if (strcmp(func, "resolve_job_workdir") == 0) {
        char *r = sf_resolve_job_workdir(&io, json_obj_get(in, "job"), json_get_str(in, "job_id", ""));
        ret = r ? json_string(r) : json_null(); free(r);
    } else if (strcmp(func, "resolve_job_reasoning_config") == 0) {
        ret = sf_resolve_job_reasoning_config(&io, json_obj_get(in, "job"),
            json_obj_get(in, "cfg"), json_get_str(in, "model", ""));
    } else if (strcmp(func, "acquire_tick_lock") == 0) {
        ret = sf_acquire_tick_lock(&io, json_get_str(in, "lock_file", ""));
    } else if (strcmp(func, "release_tick_lock") == 0) {
        ret = sf_release_tick_lock(&io, json_obj_get(in, "lock_fd"));
    } else if (strcmp(func, "maybe_reap_dead_owners") == 0) {
        sf_maybe_reap_dead_owners(&io);
        ret = json_null();
    } else if (strcmp(func, "sweep_stale_inflight_for_tick") == 0) {
        sf_sweep_stale_inflight_for_tick(&io, json_obj_get(in, "due_jobs"));
        ret = json_null();
    } else if (strcmp(func, "resolve_max_parallel_workers") == 0) {
        ret = sf_resolve_max_parallel_workers(&io);
    } else if (strcmp(func, "sweep_mcp_orphans") == 0) {
        sf_sweep_mcp_orphans(&io);
        ret = json_null();
    } else if (strcmp(func, "sweep_mcp_orphans_when_all_done") == 0) {
        sf_sweep_mcp_orphans_when_all_done(&io, json_obj_get(in, "futures"));
        ret = json_null();
    } else if (strcmp(func, "process_due_job") == 0) {
        ret = sf_process_due_job(&io, json_obj_get(in, "job"));
    } else if (strcmp(func, "record_fire_ownership_lost") == 0) {
        const char *fo = json_get_str(in, "fire_owner", NULL);
        sf_record_fire_ownership_lost(&io, json_obj_get(in, "job"),
            fo && *fo ? fo : NULL, json_get_str(in, "execution_id", ""));
        ret = json_null();
    } else if (strcmp(func, "fire_ownership_lost") == 0) {
        ret = json_bool(sf_fire_ownership_lost(&io, json_obj_get(in, "job"), json_obj_get(in, "ownership")));
    } else if (strcmp(func, "mark_incident_alerted") == 0) {
        const char *id = json_get_str(in, "incident_id", NULL);
        sf_mark_incident_alerted(&io, id && *id ? id : NULL);
        ret = json_null();
    } else if (strcmp(func, "resolve_incidents_for_recovered_job") == 0) {
        sf_resolve_incidents_for_recovered_job(&io, json_obj_get(in, "job"));
        ret = json_null();
    } else if (strcmp(func, "upsert_incident_for_failure") == 0) {
        ret = sf_upsert_incident_for_failure(&io, json_obj_get(in, "job"),
            json_get_str(in, "error", ""), json_obj_get(in, "output_file"));
    } else if (strcmp(func, "record_stale_release") == 0) {
        sf_record_stale_release(&io, json_obj_get(in, "job"), json_get_str(in, "job_id", ""),
            json_get_num(in, "age", 0), json_get_num(in, "allowance", 0), json_get_str(in, "reason", ""));
        ret = json_null();
    } else if (strcmp(func, "discard_parallel_pools") == 0) {
        sf_discard_parallel_pools(&io, json_obj_get(in, "home_keys"));
        ret = json_null();
    } else if (strcmp(func, "cron_run_scope_enter") == 0) {
        scope = json_copy(json_obj_get(in, "scope"));
        sf_cron_run_scope_enter(&io, scope);
        ret = json_null();
    } else if (strcmp(func, "cron_run_scope_exit") == 0) {
        scope = json_copy(json_obj_get(in, "scope"));
        sf_cron_run_scope_exit(&io, scope);
        ret = json_null();
    } else if (strcmp(func, "run_with_fire_claim_heartbeat") == 0) {
        ret = sf_run_with_fire_claim_heartbeat(&io, json_obj_get(in, "job"));
    } else if (strcmp(func, "compose_run_delivery") == 0) {
        ret = sf_compose_run_delivery(&io, json_obj_get(in, "job"),
            json_get_bool(in, "success", false), json_obj_get(in, "error"),
            json_get_str(in, "final_response", ""), json_obj_get(in, "output_file"),
            json_get_bool(in, "agent_declared", false));
    } else if (strcmp(func, "deliver_crash_failure") == 0) {
        ret = sf_deliver_crash_failure(&io, json_obj_get(in, "job"), json_get_str(in, "err_text", ""));
    } else {
        ret = json_object();
        json_set(ret, "error", json_string("unknown func"));
    }

    json_t *out = make_out(&io, ret, scope);
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