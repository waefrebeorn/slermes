/*
 * t_port_skills_tool_flow.c — oracle harness for the UNCLAIMED remainder of
 * tools/skills_tool.py ported in src/tools/skills_tool_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * "in" carries the injected state mirror ("state"), the scripted seam results
 * ("io") and the call arguments. The universal seam stub records one
 * {ev, args, ret} trace event per call in call order and returns the script
 * (NULL == None; a {"__raise__":...} spec is returned for the port to raise).
 * A missing/exhausted script is FAIL-CLOSED: the stub records and returns a
 * {"__raise__":"RuntimeError","message":"no script for X"} spec, matching the
 * oracle's next_script. The harness emits {"func","in","out":{ret,trace,state}}
 * per case; tests/oracle_skills_tool_flow.py drives LIVE Python identically.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "tools/skills_tool_flow.c"

typedef struct {
    const json_t *in;
    json_t *io;       /* borrowed in["io"] */
    json_t *trace;    /* owned array */
    json_t *pos;      /* owned per-name queue positions */
} skt_script_ctx;

/* Script lookup: bare array = per-call queue; {"__script__":true,"ret":v} =
 * constant list/object; anything else = constant. Absent/exhausted sets
 * *failclosed and fills fcmsg (the stub then returns a RuntimeError spec). */
static const json_t *skt_next_script(skt_script_ctx *c, const char *name,
                                     bool *failclosed, char *fcmsg, size_t fcsz) {
    *failclosed = false;
    const json_t *v = c->io ? json_obj_get(c->io, name) : NULL;
    if (!v) {
        *failclosed = true;
        snprintf(fcmsg, fcsz, "no script for %s", name);
        return NULL;
    }
    if (v->type == JSON_ARRAY) {
        double idx = json_get_num(c->pos, name, 0);
        size_t i = (size_t)idx;
        if (i >= json_len(v)) {
            *failclosed = true;
            snprintf(fcmsg, fcsz, "script exhausted for %s", name);
            return NULL;
        }
        json_set(c->pos, name, json_number((double)(i + 1)));
        return json_get(v, i);
    }
    if (v->type == JSON_OBJECT && json_get_bool(v, "__script__", false))
        return json_obj_get(v, "ret");
    return v;
}

static json_t *skt_stub_call(void *ud, const char *name, const json_t *args) {
    skt_script_ctx *c = ud;
    bool fc = false;
    char fcmsg[192];
    const json_t *script = skt_next_script(c, name, &fc, fcmsg, sizeof(fcmsg));
    json_t *ret_node;
    if (fc) ret_node = skt_raise_spec("RuntimeError", fcmsg);   /* owned */
    else ret_node = script ? json_copy(script) : json_null();
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args ? json_copy(args) : json_null());
    json_set(ev, "ret", json_copy(ret_node));
    json_append(c->trace, ev);
    if (ret_node->type == JSON_NULL) { json_free(ret_node); return NULL; }
    return ret_node;   /* owned; the port frees it (skt_call result) */
}

static void skt_state_defaults(json_t *state) {
    if (!skt_is_obj(json_obj_get(state, "skills_cache")))
        json_set(state, "skills_cache", json_object());
}

static void skt_run_case(const char *func, const json_t *in, json_t *out) {
    json_t *state = json_copy(json_obj_get(in, "state"));
    if (!skt_is_obj(state)) { json_free(state); state = json_object(); }
    skt_state_defaults(state);
    skt_script_ctx ctx = {
        .in = in, .io = json_obj_get(in, "io"),
        .trace = json_array(), .pos = json_object(),
    };
    skt_env env = { .state = state, .call = skt_stub_call, .ud = &ctx };
    json_t *ret = NULL;
    bool found = false;
    for (size_t i = 0; i < sizeof(skt_table) / sizeof(skt_table[0]); i++) {
        if (strcmp(func, skt_table[i].name) == 0) {
            ret = skt_table[i].fn(&env, in);
            found = true;
            break;
        }
    }
    if (!found) json_set(out, "error", json_string("unknown func"));
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", ctx.trace);
    json_set(out, "state", state);
    json_free(ctx.pos);
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    FILE *f = (argc > 1) ? fopen(argv[1], "rb") : stdin;
    if (!f) { fprintf(stderr, "cannot open fixture\n"); return 1; }
    char line[1 << 20];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '#') continue;
        char *err = NULL;
        json_t *c = json_parse(line, &err);
        if (err || !c || c->type != JSON_OBJECT) {
            fprintf(stderr, "bad case line: %s\n", err ? err : "not an object");
            free(err);
            json_free(c);
            continue;
        }
        const char *func = json_get_str(c, "func", "");
        const json_t *in = json_obj_get(c, "in");
        json_t *empty = json_object();
        json_t *out = json_object();
        skt_run_case(func, in ? in : empty, out);
        json_free(empty);
        char *in_s = json_serialize(in);
        char *out_s = json_serialize(out);
        json_t *func_node = json_string(func);
        char *func_s = json_serialize(func_node);
        json_free(func_node);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", func_s,
               in_s ? in_s : "null", out_s ? out_s : "null");
        free(in_s); free(out_s); free(func_s);
        json_free(out);
        json_free(c);
    }
    if (f != stdin) fclose(f);
    return 0;
}