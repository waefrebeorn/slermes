/* t_port_error_classifier_flow2.c -- harness for the FLOW2 port of the NINE
 * unclaimed leaves of agent/error_classifier.py in
 * src/agent/error_classifier_flow2.c (static ec2_* entries).
 *
 * Reads JSON LINES from stdin (or argv[1]): {"func": <live_name>, "in": {...}}
 * where in.io carries the universal-seam scripts (seam-name -> constant or
 * per-call queue) plus the scripted verdict constants. Emits JSON LINES:
 *   {"func", "in", "ret", "trace", "st"}
 * ret is the projected return (or {"__raise__":kind,"message":str(exc)}),
 * trace the ordered seam events {ev,args,ret}, st the final state projection
 * ({} on every path -- these defs mutate no caller state).
 *
 * The whole port .c is #included (area-qualified); the gate links libjson
 * alone. Script semantics (shared with the oracle Script):
 *   absent key / exhausted queue -> fail-closed RuntimeError "no script for X"
 *   non-list value               -> constant result reused every call
 *   list value                   -> per-call queue (one result per call)
 *   {"__raise__":K,"message":M}  -> raised exception of kind K
 *   {"__script__":true,"ret":X}  -> list-valued result X (queue-ambiguity wrap)
 *   null                         -> None result (delivered as NULL pointer)
 */
#define EC2_JSON_PREINCLUDED 1
#include "hermes_json.h"
#include "agent/error_classifier_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- universal-seam recording dispatcher ---- */
typedef struct {
    json_t *io;        /* borrowed in.io */
    json_t *cursors;   /* owned per-name queue cursors */
    json_t *trace;     /* owned ordered events */
} ec2_script_t;

/* drop None-valued object keys recursively (array elements kept) */
static json_t *ec2_h_strip_nulls(const json_t *v)
{
    if (!v) return json_null();
    if (v->type != JSON_OBJECT) return json_copy(v);
    json_t *o = json_object();
    for (size_t i = 0; i < v->c.count; i++) {
        json_t *val = v->c.items[i];
        if (val && val->type == JSON_NULL) continue;
        json_set(o, v->c.keys[i], ec2_h_strip_nulls(val));
    }
    return o;
}

static json_t *ec2_h_dispatch(void *ctx, const char *name, const json_t *args)
{
    ec2_script_t *sc = (ec2_script_t *)ctx;
    json_t *entry = sc->io ? json_obj_get(sc->io, name) : NULL;
    int present = entry != NULL;
    int exhausted = 0;
    json_t *result = NULL;
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
    json_t *actual = result;
    if (!failclosed && actual && actual->type == JSON_OBJECT) {
        json_t *tag = json_obj_get(actual, "__script__");
        if (tag && tag->type == JSON_BOOL && tag->bool_val)
            actual = json_obj_get(actual, "ret");
    }
    json_t *retnode;
    if (failclosed) {
        char m[200];
        snprintf(m, sizeof(m), "no script for %s", name);
        retnode = ec2_mkraise("RuntimeError", m);
    } else {
        retnode = actual ? json_copy(actual) : json_null();
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", ec2_h_strip_nulls(args));
    json_set(ev, "ret", json_copy(retnode));
    json_append(sc->trace, ev);

    if (failclosed) return retnode;
    if (!actual || actual->type == JSON_NULL) { json_free(retnode); return NULL; }
    return retnode;
}

/* ---- entry dispatch (full live name) ---- */
static json_t *ec2_h_entry(const char *func, ec2_io_t *io, const json_t *in, json_t *st)
{
#define EC2_DISPATCH(nm, fn) if (strcmp(func, nm) == 0) return fn(io, in, st)
    EC2_DISPATCH("_is_byte_size_limit_message", ec2_is_byte_size_limit_message_entry);
    EC2_DISPATCH("is_reasoning_level_rejection", ec2_is_reasoning_level_rejection_entry);
    EC2_DISPATCH("is_reasoning_required_rejection", ec2_is_reasoning_required_rejection_entry);
    EC2_DISPATCH("is_reasoning_field_rejection", ec2_is_reasoning_field_rejection_entry);
    EC2_DISPATCH("_account_gates_model", ec2_account_gates_model_entry);
    EC2_DISPATCH("_by_message", ec2_by_message_entry);
    EC2_DISPATCH("_has_large_inline_image", ec2_has_large_inline_image_entry);
    EC2_DISPATCH("_oversized_message_content_rejection", ec2_oversized_message_content_rejection_entry);
    EC2_DISPATCH("_classify_image_tool_422", ec2_classify_image_tool_422_entry);
#undef EC2_DISPATCH
    return ec2_mkraise("RuntimeError", "unknown func");
}

/* ---- line reader (\n only) ---- */
static char *ec2_h_readline(FILE *f)
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

static void ec2_h_run(const char *func, json_t *in)
{
    ec2_script_t sc;
    sc.io = ec2_g(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();
    ec2_io_t io;
    io.call = ec2_h_dispatch;
    io.ctx = &sc;
    io.data = sc.io;
    json_t *st = json_object();
    json_t *in_echo = json_copy(in);
    json_t *ret = ec2_h_entry(func, &io, in, st);
    if (ec2_is_raise(ret)) {
        json_free(st);
        st = json_object();
    }
    json_t *out = json_object();
    json_set(out, "func", json_string(func));
    json_set(out, "in", in_echo);
    json_set(out, "ret", ret ? ret : json_null());
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
    while ((line = ec2_h_readline(f)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec) continue;
        const char *func = ec2_s(ec2_g(rec, "func"));
        json_t *in = ec2_g(rec, "in");
        if (func && in) ec2_h_run(func, in);
        json_free(rec);
    }
    if (f != stdin) fclose(f);
    return 0;
}
