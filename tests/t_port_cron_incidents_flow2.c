/* t_port_cron_incidents_flow2.c — harness for the cron/incidents.py FLOW2 port.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out": {"ret","trace","state"}}.
 * "trace" is the ordered seam-call NAME sequence; "state" the mutated module
 * globals. Scripted I/O lives in in.io: seam-name -> result, where a result
 * is {"ret": <value>, "state": {...}, "raise": {"type","message"}} (keys
 * optional). A LIST result is a per-call queue; a single object is reused.
 * Mirrors tests/oracle_cron_incidents_flow2.py's recording stubs exactly.
 */
#include "hermes_json.h"
#include "cron/cron_incidents_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    json_t *io;
    json_t *cursors;
} script_ctx_t;

static json_t *script_next(script_ctx_t *sc, const char *name) {
    if (!sc || !sc->io) return NULL;
    json_t *entry = json_obj_get(sc->io, name);   /* NULL == missing script */
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

static json_t *dispatch(ci2_io_t *io, const char *name, json_t *args) {
    (void)args;                          /* name-only trace */
    script_ctx_t *sc = (script_ctx_t *)io->ctx;
    json_t *result = script_next(sc, name);
    apply_state_effects(io->state, result);
    if (result && result->type == JSON_OBJECT) {
        json_t *rs = json_obj_get(result, "raise");
        if (!rs || rs->type != JSON_OBJECT) rs = json_obj_get(result, "__raise__");
        const char *type = NULL;
        const char *msg = NULL;
        json_t *en = NULL;
        if (rs && rs->type == JSON_OBJECT) {
            type = json_get_str(rs, "type", json_get_str(rs, "__raise__", ""));
            msg = json_get_str(rs, "message", "");
            en = json_obj_get(rs, "errno");
        } else if (rs && rs->type == JSON_STRING) {
            type = rs->str_val;
            msg = json_get_str(result, "message", "");
        }
        if (type) {
            json_t *exc = json_object();
            json_set(exc, "__raise__", json_string(type));
            json_set(exc, "message", json_string(msg ? msg : ""));
            if (en) json_set(exc, "errno", json_copy(en));
            return exc;
        }
        json_t *ret = json_obj_get(result, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return json_null();
}

static void io_init(ci2_io_t *io, script_ctx_t *sc, json_t *in, json_t *state) {
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

static json_t *make_out(ci2_io_t *io, json_t *ret) {
    json_t *out = json_object();
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", io->trace ? json_copy(io->trace) : json_array());
    json_set(out, "state", io->state ? json_copy(io->state) : json_object());
    return out;
}

static json_t *run_case(const char *func, json_t *in) {
    ci2_io_t io;
    script_ctx_t sc;
    json_t *state = state_from(in);
    io_init(&io, &sc, in, state);
    json_t *ret = NULL;

    if (strcmp(func, "db_path") == 0) {
        ret = ci2_db_path(&io, json_obj_get(in, "module_file"));
    } else if (strcmp(func, "connect") == 0) {
        ret = ci2_connect(&io);
    } else if (strcmp(func, "initialize_schema") == 0) {
        ret = ci2_initialize_schema(&io);
    } else if (strcmp(func, "transaction") == 0) {
        ret = ci2_transaction(&io);
    } else if (strcmp(func, "set_incident_state") == 0) {
        ret = ci2_set_incident_state(&io, json_obj_get(in, "inc_state"));
    } else if (strcmp(func, "ack_incident") == 0) {
        ret = ci2_ack_incident(&io, json_obj_get(in, "incident_id"));
    } else if (strcmp(func, "close_incidents") == 0) {
        ret = ci2_close_incidents(&io, json_obj_get(in, "job_id"));
    } else if (strcmp(func, "state_filter") == 0) {
        ret = ci2_state_filter(json_obj_get(in, "inc_state"));
    } else if (strcmp(func, "count_incidents") == 0) {
        ret = ci2_count_incidents(&io, json_obj_get(in, "inc_state"));
    } else {
        ret = json_object();
        json_set(ret, "error", json_string("unknown func"));
    }

    json_t *out = make_out(&io, ret);
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
