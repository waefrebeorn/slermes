/* t_port_context_compressor_flow4.c — oracle harness for the fourth-wave FLOW
 * port of agent/context_compressor.py (src/agent/context_compressor_flow4.c).
 *
 * Reads JSON LINES from stdin (or argv[1]): {"func": <live_name>, "in": {...}}
 * where in.io carries the universal-seam scripts (seam-name -> constant or
 * per-call queue). Emits JSON LINES:
 *   {"func","in","ret","trace","st"}
 *
 * Fail-closed: an absent/exhausted script returns a RuntimeError raise spec
 * (recorded as the event ret BEFORE propagation). A bare JSON array script is
 * a per-call QUEUE; a list RESULT must be wrapped {"__script__":true,"ret":[...]}.
 * A scripted null returns a NULL pointer (None).
 *
 * Gate: bash tests/run_one_oracle.sh context_compressor_flow4 \
 *           oracle_context_compressor_flow4.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agent/context_compressor_flow4.c"

/* ---- universal-seam recording dispatcher ---- */
typedef struct {
    const json_t *io;    /* borrowed in.io */
    json_t *cursors;     /* owned per-name queue cursors */
    json_t *trace;       /* owned ordered events */
} cc4_script_t;

/* drop None-valued object keys recursively (array elements kept) */
static json_t *cc4_h_strip_nulls(const json_t *v)
{
    if (!v) return json_null();
    if (v->type != JSON_OBJECT) return json_copy(v);
    json_t *o = json_object();
    for (size_t i = 0; i < v->c.count; i++) {
        json_t *val = v->c.items[i];
        if (val && val->type == JSON_NULL) continue;
        json_set(o, v->c.keys[i], cc4_h_strip_nulls(val));
    }
    return o;
}

static json_t *cc4_h_dispatch(void *ctx, const char *name, const json_t *args)
{
    cc4_script_t *sc = (cc4_script_t *)ctx;
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
        char *m = cc4_xfmt("no script for %s", name);
        retnode = cc4_mkraise("RuntimeError", m ? m : "no script");
        free(m);
    } else {
        retnode = actual ? json_copy(actual) : json_null();
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", cc4_h_strip_nulls(args));
    json_set(ev, "ret", json_copy(retnode));
    json_append(sc->trace, ev);

    if (failclosed) return retnode;
    if (!actual || actual->type == JSON_NULL) { json_free(retnode); return NULL; }
    return retnode;
}

/* ---- entry dispatch ---- */
typedef json_t *(*cc4_fn_t)(cc4_io_t *, const json_t *, json_t *);
typedef struct { const char *func; cc4_fn_t fn; } cc4_dispatch_t;
static const cc4_dispatch_t CC4_DISPATCH[] = {
    { "_effective_input_window", cc4_effective_input_window },
    { "_model_threshold_key_rank", cc4_model_threshold_key_rank },
    { "_is_text_only_content", cc4_is_text_only_content },
    { "_is_clarify_non_response", cc4_is_clarify_non_response },
    { "_restated_request_text", cc4_restated_request_text },
    { "_authored_request_text", cc4_authored_request_text },
    { "_pending_tool_round", cc4_pending_tool_round },
    { "_image_payload", cc4_image_payload },
    { "_sum_skill_view", cc4_sum_skill_view },
    { "_collect_paths_from_jsonish", cc4_collect_paths_from_jsonish },
    { "_sum_clarify", cc4_sum_clarify },
};
#define CC4_DISPATCH_N (sizeof(CC4_DISPATCH) / sizeof(CC4_DISPATCH[0]))

/* ---- line reader (\n only) ---- */
static char *cc4_h_readline(FILE *f)
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

static void cc4_h_run(const char *func, const json_t *in)
{
    cc4_script_t sc;
    sc.io = cc4_get(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();
    cc4_io_t io;
    io.call = cc4_h_dispatch;
    io.ctx = &sc;

    json_t *st = json_object();
    const json_t *sj = cc4_get(in, "state");
    if (cc4_is_obj(sj)) { json_free(st); st = json_copy(sj); }

    json_t *in_echo = json_copy(in);
    json_t *ret = NULL;
    int found = 0;
    for (size_t i = 0; i < CC4_DISPATCH_N; i++) {
        if (strcmp(CC4_DISPATCH[i].func, func) == 0) { ret = CC4_DISPATCH[i].fn(&io, in, st); found = 1; break; }
    }
    if (!found) ret = cc4_mkraise("RuntimeError", "unknown func");
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
    json_free(out);   /* owns in_echo / ret / trace / st */
    json_free(sc.cursors);
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    FILE *f = stdin;
    if (argc > 1) { f = fopen(argv[1], "rb"); if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; } }
    char *line;
    while ((line = cc4_h_readline(f)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec) continue;
        const char *func = cc4_str(cc4_get(rec, "func"));
        const json_t *in = cc4_get(rec, "in");
        if (func && in) cc4_h_run(func, in);
        json_free(rec);
    }
    if (f != stdin) fclose(f);
    return 0;
}
