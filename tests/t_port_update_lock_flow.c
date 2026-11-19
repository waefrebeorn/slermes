/* t_port_update_lock_flow.c — harness for the update-lock FLOW port.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam calls {"name","args"} in "trace"), the final return
 * ("ret"), and — for the claim state machine — the mutated self/claims state
 * ("state"). The echoed "in" is the ORIGINAL input (the oracle re-drives the
 * live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim), a LIST consumed in call order (a
 * queue), or {"__script__": true, "ret": <value>, "raise": {type,message,errno?},
 * "holder": <holder spec>}. "holder" is the self.holder state effect of the
 * seamed UpdateLock._adopt_or_refuse refusal — the SAME effect the oracle's
 * recording stub applies. A missing/exhausted script is a fail-closed
 * RuntimeError on both sides.
 */
#include "hermes_json.h"
#include "port_update_lock_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    json_t *io_map;
    json_t *cursors;
    json_t *self;   /* borrowed pin.self: the _adopt_or_refuse holder effect */
} script_ctx_t;

static json_t *make_exc(json_t *spec) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", json_get_str(spec, "type", "RuntimeError"));
    json_set_hdr(e, "message", json_get_str(spec, "message", ""));
    json_t *en = json_obj_get(spec, "errno");
    if (en && en->type == JSON_NUMBER) json_set(e, "errno", json_copy(en));
    return e;
}

static json_t *holder_norm(json_t *spec) {
    json_t *h = json_object();
    json_set(h, "pid", json_number(json_get_num(spec, "pid", 0)));
    json_set(h, "age_seconds", json_number(json_get_num(spec, "age_seconds", 0.0)));
    json_t *reason = json_obj_get(spec, "reason");
    json_set(h, "reason", (reason && reason->type != JSON_NULL) ? json_copy(reason) : json_null());
    json_set(h, "held", json_bool(json_get_bool(spec, "held", false)));
    return h;
}

static json_t *entry_result(json_t *entry, const char *name, script_ctx_t *sc) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return make_exc(raise);
        /* the seamed _adopt_or_refuse assigns self.holder on refusal */
        if (strcmp(name, "self._adopt_or_refuse") == 0 && sc->self) {
            json_t *h = json_obj_get(entry, "holder");
            if (h && h->type == JSON_OBJECT) json_set(sc->self, "holder", holder_norm(h));
        }
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

static json_t *dispatch(ulf_io_t *io, const char *name, json_t *args) {
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
        return entry_result(json_get(entry, idx), name, sc);
    }
    return entry_result(entry, name, sc);
}

typedef json_t *(*ulf_fn_t)(ulf_io_t *, json_t *);
typedef struct { const char *func; ulf_fn_t fn; bool stateful; } ulf_case_t;

static const ulf_case_t CASES[] = {
    { "_bounded_int", ulf_bounded_int, false },
    { "_as_ct", ulf_as_ct, false },
    { "_incarnation", ulf_incarnation, false },
    { "_identity_live", ulf_identity_live, false },
    { "_parse_marker", ulf_parse_marker, false },
    { "judge_marker", ulf_judge_marker, false },
    { "_release_decision", ulf_release_decision, false },
    { "_live_partners", ulf_live_partners, false },
    { "_reclaim_dead", ulf_reclaim_dead, false },
    { "_claim_marker", ulf_claim_marker, true },
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

        const ulf_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            out = json_object();
            json_set_hdr(out, "error", "unknown func");
        } else {
            ulf_io_t io;
            script_ctx_t sc;
            io.call = dispatch;
            io.trace = json_array();
            io.ctx = &sc;
            sc.io_map = json_obj_get(in, "io");
            sc.cursors = json_object();
            json_t *pin = json_copy(in);
            sc.self = json_obj_get(pin, "self");
            json_t *r = tc->fn(&io, pin);
            out = json_object();
            json_set(out, "ret", r ? r : json_null());
            json_set(out, "trace", json_copy(io.trace));
            if (tc->stateful) {
                json_t *state = json_object();
                json_t *self = json_obj_get(pin, "self");
                json_t *claims = json_obj_get(pin, "claims");
                json_set(state, "self", self ? json_copy(self) : json_null());
                json_set(state, "claims", claims ? json_copy(claims) : json_null());
                json_set(out, "state", state);
            }
            json_free(pin);
            json_free(io.trace);
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
