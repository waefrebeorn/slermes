/* t_port_scheduler_flow2.c — harness for the cron/scheduler.py UNCLAIMED-remainder
 * FLOW port (src/agent/scheduler_flow2.c).
 *
 * Self-contained: #includes the port .c by AREA-QUALIFIED name. Reads JSON LINES
 * from stdin (tests/fixtures/scheduler_flow2.jsonl):
 *   {"func": <live_name>, "in": {...}}
 * Emits JSON LINES: {"func", "in", "out"} where out captures the ordered seam
 * call sequence ("trace": [{"name","args"}]), the final return ("ret"), and the
 * mutated module-global state ("state"). The echoed "in" is the ORIGINAL input
 * (the oracle re-drives live from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either a
 * plain JSON value (returned verbatim), a LIST consumed in call order (a queue),
 * or {"__script__": true, "ret": <value>, "raise": {type,message,errno?}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 * in.state seeds the working module-global state; in.import_fail gates imports.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "agent/scheduler_flow2.c"

typedef struct {
    json_t *io_map;
    json_t *cursors;
} script_ctx_t;

static json_t *make_exc(json_t *spec) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", json_get_str(spec, "type", "RuntimeError"));
    json_set_hdr(e, "message", json_get_str(spec, "message", ""));
    json_t *en = json_obj_get(spec, "errno");
    if (en && en->type == JSON_NUMBER) json_set(e, "errno", json_copy(en));
    return e;
}

static json_t *entry_result(json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

static json_t *dispatch(sc2_io_t *io, const char *name, json_t *args) {
    (void)args;
    script_ctx_t *sc = (script_ctx_t *)io->ctx;
    json_t *entry = sc->io_map ? json_obj_get(sc->io_map, name) : NULL;
    if (!entry) {
        json_t *e = json_object();
        json_set_hdr(e, "__raise__", "RuntimeError");
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        json_set_hdr(e, "message", msg);
        return e;
    }
    if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) {
            json_t *e = json_object();
            json_set_hdr(e, "__raise__", "RuntimeError");
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            json_set_hdr(e, "message", msg);
            return e;
        }
        json_set(sc->cursors, name, json_number(cur + 1));
        return entry_result(json_get(entry, idx));
    }
    return entry_result(entry);
}

typedef json_t *(*sc2_fn_t)(sc2_io_t *, json_t *);
typedef struct { const char *func; sc2_fn_t fn; } sc2_case_t;

static const sc2_case_t CASES[] = {
    { "_job_fallback_chain", sc2_job_fallback_chain },
    { "_current_gateway_code_sha", sc2_current_gateway_code_sha },
    { "_remember_inflight_home", sc2_remember_inflight_home },
    { "_inflight_home_path", sc2_inflight_home_path },
    { "get_running_job_details", sc2_get_running_job_details },
    { "_wedged_inflight_keys", sc2_wedged_inflight_keys },
    { "get_wedged_job_ids", sc2_get_wedged_job_ids },
    { "get_restart_wait_cron_counts", sc2_get_restart_wait_cron_counts },
    { "_record_external_cron_worker", sc2_record_external_cron_worker },
    { "_cron_interval_minutes", sc2_cron_interval_minutes },
    { "_latest_executions_for_releasable_claims", sc2_latest_executions_for_releasable_claims },
    { "_row_belongs_to_claim", sc2_row_belongs_to_claim },
    { "_reclaim_fds_best_effort", sc2_reclaim_fds_best_effort },
    { "_load_prefill_messages", sc2_load_prefill_messages },
    { "_preflight_or_block", sc2_preflight_or_block },
    { "_blocked_config_result", sc2_blocked_config_result },
    { "_load_credential_pool", sc2_load_credential_pool },
    { "_init_cron_mcp_tools", sc2_init_cron_mcp_tools },
    { "_raise_inactivity_timeout", sc2_raise_inactivity_timeout },
    { "_run_doc_header", sc2_run_doc_header },
    { "_reload_dotenv_and_publish_delivery_target", sc2_reload_dotenv_and_publish_delivery_target },
    { "_install_fire_secret_scope", sc2_install_fire_secret_scope },
    { "_reset_fire_secret_scope", sc2_reset_fire_secret_scope },
    { "_start_owned_run", sc2_start_owned_run },
    { "_maybe_run_worktree_maintenance", sc2_maybe_run_worktree_maintenance },
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

static char *read_line(FILE *f) {
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    int c;
    while ((c = fgetc(f)) != EOF && c != '\n') {
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        buf[len++] = (char)c;
    }
    if (c == EOF && len == 0) { free(buf); return NULL; }
    buf[len] = '\0';
    return buf;
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    char *line;
    while ((line = read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in) in = json_object();
        json_t *out = NULL;

        const sc2_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            out = json_object();
            json_set_hdr(out, "error", "unknown func");
        } else {
            sc2_io_t io;
            script_ctx_t sc;
            io.call = dispatch;
            io.trace = json_array();
            json_t *st = json_obj_get(in, "state");
            io.state = (st && st->type == JSON_OBJECT) ? json_copy(st) : json_object();
            io.ctx = &sc;
            sc.io_map = json_obj_get(in, "io");
            sc.cursors = json_object();
            json_t *pin = json_copy(in);
            json_t *r = tc->fn(&io, pin);
            out = json_object();
            json_set(out, "ret", r ? r : json_null());
            json_set(out, "trace", json_copy(io.trace));
            json_set(out, "state", json_copy(io.state));
            json_free(pin);
            json_free(io.trace);
            json_free(io.state);
            json_free(sc.cursors);
        }

        json_t *line_out = json_object();
        json_set_hdr(line_out, "func", func);
        json_set(line_out, "in", json_copy(in));
        json_set(line_out, "out", out ? out : json_null());
        char *ser = json_serialize(line_out);
        printf("%s\n", ser ? ser : "{}");
        free(ser);
        json_free(line_out);
        json_free(rec);
    }
    return 0;
}
