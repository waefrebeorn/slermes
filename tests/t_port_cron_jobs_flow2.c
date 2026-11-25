/* t_port_cron_jobs_flow2.c — harness for the cron/jobs.py FLOW2 port.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out": {"ret","trace","state"[,echo...]}}.
 * "trace" is the ordered seam-call NAME sequence; "state" the mutated module
 * globals. Scripted I/O lives in in.io: seam-name -> result, where a result
 * is {"ret": <value>, "state": {...}, "raise": {"type","message"}} (keys
 * optional). A LIST result is a per-call queue; a single object is reused.
 * Mirrors tests/oracle_cron_jobs_flow2.py's recording stubs exactly.
 */
#include "hermes_json.h"
#include "cron/cron_jobs_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static json_t *dispatch(cj2_io_t *io, const char *name, json_t *args) {
    (void)args;  /* name-only trace */
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

static void io_init(cj2_io_t *io, script_ctx_t *sc, json_t *in, json_t *state) {
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

static json_t *make_out(cj2_io_t *io, json_t *ret) {
    json_t *out = json_object();
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", io->trace ? json_copy(io->trace) : json_array());
    json_set(out, "state", io->state ? json_copy(io->state) : json_object());
    return out;
}

static json_t *copy_or_obj(json_t *v) {
    json_t *o = v ? json_copy(v) : NULL;
    return o ? o : json_object();
}

static json_t *run_case(const char *func, json_t *in) {
    cj2_io_t io;
    script_ctx_t sc;
    json_t *state = state_from(in);
    io_init(&io, &sc, in, state);
    json_t *ret = NULL;
    json_t *echo1 = NULL;
    json_t *echo2 = NULL;

    if (strcmp(func, "normalize_job_updates") == 0) {
        echo1 = copy_or_obj(json_obj_get(in, "updates"));
        ret = cj2_normalize_job_updates(&io, json_obj_get(in, "job"), echo1);
    } else if (strcmp(func, "rederive_repeat") == 0) {
        echo1 = copy_or_obj(json_obj_get(in, "updates"));
        ret = cj2_rederive_repeat(&io, json_obj_get(in, "job"), echo1);
    } else if (strcmp(func, "apply_schedule_update") == 0) {
        echo1 = copy_or_obj(json_obj_get(in, "updated"));
        echo2 = copy_or_obj(json_obj_get(in, "updates"));
        ret = cj2_apply_schedule_update(&io, echo1, echo2,
                                        json_get_str(in, "job_id", ""));
    } else if (strcmp(func, "fill_missing_next_run") == 0) {
        echo1 = copy_or_obj(json_obj_get(in, "updated"));
        ret = cj2_fill_missing_next_run(&io, echo1);
    } else if (strcmp(func, "cron_config_number") == 0) {
        ret = cj2_cron_config_number(&io);
    } else if (strcmp(func, "record_run_outcome") == 0) {
        echo1 = copy_or_obj(json_obj_get(in, "job"));
        ret = cj2_record_run_outcome(&io, echo1,
                    json_get_bool(in, "success", false),
                    json_obj_get(in, "error"),
                    json_obj_get(in, "delivery_error"),
                    json_obj_get(in, "status"),
                    json_obj_get(in, "now"));
    } else if (strcmp(func, "advance_after_run") == 0) {
        echo1 = copy_or_obj(json_obj_get(in, "job"));
        ret = cj2_advance_after_run(&io, echo1,
                    json_obj_get(in, "now"),
                    json_get_bool(in, "ladder_rung", false));
    } else if (strcmp(func, "under_fire_fence") == 0) {
        ret = cj2_under_fire_fence(&io);
    } else {
        ret = json_object();
        json_set(ret, "error", json_string("unknown func"));
    }

    json_t *out = make_out(&io, ret);
    if (echo1) json_set(out, "job", json_null()); /* placeholder, overwritten below */
    /* Re-label echo fields consistently with the oracle. */
    if (echo1) json_obj_del(out, "job");
    if (strcmp(func, "normalize_job_updates") == 0 || strcmp(func, "rederive_repeat") == 0)
        json_set(out, "updates", echo1);
    else if (strcmp(func, "apply_schedule_update") == 0) {
        json_set(out, "updated", echo1); json_set(out, "updates", echo2);
    } else if (strcmp(func, "fill_missing_next_run") == 0)
        json_set(out, "updated", echo1);
    else if (strcmp(func, "record_run_outcome") == 0 || strcmp(func, "advance_after_run") == 0)
        json_set(out, "job", echo1);
    else {
        if (echo1) json_free(echo1);
        if (echo2) json_free(echo2);
    }
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
