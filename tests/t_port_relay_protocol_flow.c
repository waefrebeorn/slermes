/* t_port_relay_protocol_flow.c — oracle harness for the UNCLAIMED-remainder
 * FLOW port of the relay wire protocol (src/gateway/relay_protocol_flow.c;
 * defining module gateway/relay/ws_transport.py — gateway/relay/protocol.py
 * does not exist, transport.py is the pure Protocol surface).
 *
 * Reads JSON LINES from stdin (or argv[1]): {"func": <live_name>, "in": {...}}
 * where in.io carries the universal-seam scripts (seam-name -> constant or
 * per-call queue) and in.state carries the mutable carrier. Emits JSON LINES:
 *   {"func", "in", "ret", "trace", "st"}
 * ret is the projected return OR a raise spec {"__raise__":kind,"message":...};
 * trace the ordered seam events {ev,args,ret}; st the final state window.
 *
 * Fail-closed: an absent / exhausted script returns a RuntimeError raise spec
 * ("no script for <name>"), recorded as the event ret BEFORE propagation. A
 * bare JSON array script is a per-call QUEUE; a list RESULT must be wrapped
 * {"__script__":true,"ret":[...]}. A scripted null returns a NULL pointer
 * (None), not a json_null node.
 *
 * Gate: bash tests/run_one_oracle.sh relay_protocol_flow \
 *           oracle_relay_protocol_flow.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gateway/relay_protocol_flow.c"

/* ---- universal-seam recording dispatcher ---- */
typedef struct {
    const json_t *io;    /* borrowed in.io */
    json_t *cursors;     /* owned per-name queue cursors */
    json_t *trace;       /* owned ordered events */
} rp_script_t;

/* drop None-valued object keys recursively (array elements kept) */
static json_t *rp_h_strip_nulls(const json_t *v)
{
    if (!v) return json_null();
    if (v->type != JSON_OBJECT) return json_copy(v);
    json_t *o = json_object();
    for (size_t i = 0; i < v->c.count; i++) {
        json_t *val = v->c.items[i];
        if (val && val->type == JSON_NULL) continue;
        json_set(o, v->c.keys[i], rp_h_strip_nulls(val));
    }
    return o;
}

static json_t *rp_h_dispatch(void *ctx, const char *name, const json_t *args)
{
    rp_script_t *sc = (rp_script_t *)ctx;
    const json_t *entry = sc->io ? json_obj_get(sc->io, name) : NULL;
    int present = entry != NULL;
    int exhausted = 0;
    const json_t *result = NULL;
    if (present) {
        if (entry->type == JSON_ARRAY) {
            double cur = json_get_num(sc->cursors, name, 0);
            size_t idx = (size_t)cur;
            if (idx >= json_len(entry)) exhausted = 1;
            else {
                result = json_get(entry, idx);
                json_set(sc->cursors, name, json_number(cur + 1));
            }
        } else {
            result = entry;
        }
    }
    int failclosed = !present || exhausted;
    const json_t *actual = result;
    if (!failclosed && actual && actual->type == JSON_OBJECT) {
        const json_t *tag = json_obj_get(actual, "__script__");
        if (tag && tag->type == JSON_BOOL && tag->bool_val)
            actual = json_obj_get(actual, "ret");
    }
    json_t *retnode;
    if (failclosed) {
        char *m = rp_xfmt("no script for %s", name);
        retnode = rp_mkraise("RuntimeError", m ? m : "no script");
        free(m);
    } else {
        retnode = actual ? json_copy(actual) : json_null();
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", rp_h_strip_nulls(args));
    json_set(ev, "ret", json_copy(retnode));
    json_append(sc->trace, ev);

    if (failclosed) return retnode;
    if (!actual || actual->type == JSON_NULL) { json_free(retnode); return NULL; }
    return retnode;
}

/* ---- per-func state materialization (mirrors WebSocketRelayTransport.__init__
 * defaults for the keys each flow reads; __init__ flows build st fresh) ---- */
static int rp_h_is_init(const char *func)
{
    return strcmp(func, "__init__") == 0 || strcmp(func, "_PendingRequest.__init__") == 0;
}
static void rp_h_def(json_t *st, const char *key, json_t *val)
{
    if (!json_obj_get(st, key)) json_set(st, key, val);
    else json_free(val);
}
static json_t *rp_h_default_event(void)
{
    json_t *ev = json_object();
    json_set(ev, "__event__", json_bool(true));
    json_set(ev, "id", json_string("ev0"));
    json_set(ev, "set", json_bool(false));
    return ev;
}
static void rp_h_materialize(const char *func, const json_t *in, json_t *st)
{
    if (rp_h_is_init(func)) return;   /* fresh st built by the port */
    if (strcmp(func, "_settle_connection_lost") == 0) {
        const json_t *f = rp_get(in, "fut");
        json_set(st, "fut", f ? json_copy(f) : json_null());
        return;
    }
    if (strcmp(func, "_fail_pending") == 0 || strcmp(func, "_on_outbound_result") == 0) {
        rp_h_def(st, "_pending", json_object());
    } else if (strcmp(func, "_on_descriptor") == 0) {
        rp_h_def(st, "_descriptors_by_platform", json_object());
        rp_h_def(st, "_descriptor", json_null());
        rp_h_def(st, "_descriptor_ready", json_null());
        rp_h_def(st, "_handshake_succeeded", json_bool(false));
    } else if (strcmp(func, "_dialer_running") == 0) {
        rp_h_def(st, "_supervisor", json_null());
    } else if (strcmp(func, "_latch_if_fresh_token_refused") == 0) {
        rp_h_def(st, "_auth_retry_pending", json_bool(false));
        rp_h_def(st, "_auth_revoked", json_bool(false));
    } else if (strcmp(func, "hold_redial") == 0 || strcmp(func, "release_redial") == 0) {
        rp_h_def(st, "_redial_held", json_bool(false));
        rp_h_def(st, "_redial_release", rp_h_default_event());
    } else if (strcmp(func, "_await_redial_hold") == 0) {
        rp_h_def(st, "_redial_held", json_bool(false));
        rp_h_def(st, "_redial_release", rp_h_default_event());
        rp_h_def(st, "_redial_hold_max_s", json_number(60.0));
    } else if (strcmp(func, "_redial_with_fresh_token") == 0) {
        rp_h_def(st, "_closing", json_bool(false));
        rp_h_def(st, "_reconnect", json_bool(false));
        rp_h_def(st, "_supervisor", json_null());
        rp_h_def(st, "_auth_retry_pending", json_bool(false));
        rp_h_def(st, "_auth_revoked", json_bool(false));
    } else if (strcmp(func, "_on_inbound") == 0) {
        rp_h_def(st, "_inbound", json_null());
    } else if (strcmp(func, "_ack_admitted") == 0) {
        rp_h_def(st, "_inbound_acks", json_array());
    } else if (strcmp(func, "_on_going_idle_ack") == 0) {
        rp_h_def(st, "_going_idle_ack", json_null());
    } else if (strcmp(func, "_on_interrupt_inbound") == 0) {
        rp_h_def(st, "_interrupt_inbound_handler", json_null());
    } else if (strcmp(func, "_on_passthrough_forward") == 0) {
        rp_h_def(st, "_passthrough_handler", json_null());
    }
}

/* ---- line reader (\n only) ---- */
static char *rp_h_readline(FILE *f)
{
    size_t cap = 4096, n = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c == '\n') break;
        if (n + 1 >= cap) { cap *= 2; char *nb = (char *)realloc(buf, cap); if (!nb) { free(buf); return NULL; } buf = nb; }
        buf[n++] = (char)c;
    }
    if (c == EOF && n == 0) { free(buf); return NULL; }
    buf[n] = '\0';
    return buf;
}

static void rp_h_run(const char *func, const json_t *in)
{
    rp_script_t sc;
    sc.io = rp_get(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();
    rp_io_t io;
    io.call = rp_h_dispatch;
    io.ctx = &sc;

    json_t *st = json_object();
    if (!rp_h_is_init(func)) {
        const json_t *sj = rp_get(in, "state");
        if (rp_is_obj(sj)) { json_free(st); st = json_copy(sj); }
    }
    rp_h_materialize(func, in, st);

    json_t *in_echo = json_copy(in);
    json_t *ret = NULL;
    int found = 0;
    for (size_t i = 0; i < RP_DISPATCH_N; i++) {
        if (strcmp(RP_DISPATCH[i].func, func) == 0) { ret = RP_DISPATCH[i].fn(&io, in, st); found = 1; break; }
    }
    if (!found) ret = rp_mkraise("RuntimeError", "unknown func");
    else if (!ret) ret = json_null();

    json_t *out = json_object();
    json_set(out, "func", json_string(func));
    json_set(out, "in", in_echo);
    json_set(out, "ret", ret);
    json_set(out, "trace", sc.trace);
    json_set(out, "st", st);
    char *s = json_serialize(out);
    printf("%s\n", s ? s : "{}");
    free(s);
    json_free(out);
    json_free(sc.cursors);
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    FILE *f = stdin;
    if (argc > 1) { f = fopen(argv[1], "rb"); if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; } }
    char *line;
    while ((line = rp_h_readline(f)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec) continue;
        const char *func = rp_str(rp_get(rec, "func"));
        const json_t *in = rp_get(rec, "in");
        if (func && in) rp_h_run(func, in);
        json_free(rec);
    }
    if (f != stdin) fclose(f);
    return 0;
}
