/* t_port_context_compressor_flow5.c — oracle harness for the fifth-wave FLOW
 * port of agent/context_compressor.py (src/agent/context_compressor_flow5.c).
 *
 * Reads JSON LINES from stdin (or argv[1]): {"func": <live_name>, "in": {...}}
 * where in.io carries the universal-seam scripts (seam-name -> constant or
 * per-call queue). Emits JSON LINES: {"func","in","ret","trace","st"}.
 *
 * Fail-closed: an absent/exhausted script returns a RuntimeError raise spec
 * (recorded as the event ret BEFORE propagation). A bare JSON array script is
 * a per-call QUEUE; a list RESULT must be wrapped {"__script__":true,"ret":[...]}.
 * A scripted null returns a NULL pointer (None).
 *
 * Gate: bash tests/run_one_oracle.sh context_compressor_flow5 \
 *           oracle_context_compressor_flow5.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agent/context_compressor_flow5.c"

/* ---- universal-seam recording dispatcher ---- */
typedef struct {
    const json_t *io;    /* borrowed in.io */
    json_t *cursors;     /* owned per-name queue cursors */
    json_t *trace;       /* owned ordered events */
} cc5_script_t;

/* drop None-valued object keys recursively (array elements kept) */
static json_t *cc5_h_strip_nulls(const json_t *v)
{
    if (!v) return json_null();
    if (v->type != JSON_OBJECT) return json_copy(v);
    json_t *o = json_object();
    for (size_t i = 0; i < v->c.count; i++) {
        json_t *val = v->c.items[i];
        if (val && val->type == JSON_NULL) continue;
        json_set(o, v->c.keys[i], cc5_h_strip_nulls(val));
    }
    return o;
}

static json_t *cc5_h_dispatch(void *ctx, const char *name, const json_t *args)
{
    cc5_script_t *sc = (cc5_script_t *)ctx;
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
        char *m = cc5_xfmt("no script for %s", name);
        retnode = cc5_mkraise("RuntimeError", m ? m : "no script");
        free(m);
    } else {
        retnode = actual ? json_copy(actual) : json_null();
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", cc5_h_strip_nulls(args));
    json_set(ev, "ret", json_copy(retnode));
    json_append(sc->trace, ev);

    if (failclosed) return retnode;
    if (!actual || actual->type == JSON_NULL) { json_free(retnode); return NULL; }
    return retnode;
}

/* ---- entry dispatch ---- */
typedef json_t *(*cc5_fn_t)(cc5_io_t *, const json_t *, json_t *);
typedef struct { const char *func; cc5_fn_t fn; } cc5_dispatch_t;
static const cc5_dispatch_t CC5_DISPATCH[] = {
    { "_is_image_part", cc5_is_image_part },
    { "_content_has_images", cc5_content_has_images },
    { "_replace_image_parts", cc5_replace_image_parts },
    { "_strip_images_from_content", cc5_strip_images_from_content },
    { "_image_part_label", cc5_image_part_label },
    { "_str_arg", cc5_str_arg },
    { "_tool_content_has_images", cc5_tool_content_has_images },
    { "_strip_images_from_tool_msg", cc5_strip_images_from_tool_msg },
    { "_filter_legacy_clarify_answers", cc5_filter_legacy_clarify_answers },
    { "_memory_provider_section", cc5_memory_provider_section },
    { "resolve_model_threshold", cc5_resolve_model_threshold },
    { "_compact_fallback_turn", cc5_compact_fallback_turn },
    { "is_user_originated_turn", cc5_is_user_originated_turn },
    { "fallback_reason", cc5_fallback_reason },
    { "_effective_threshold_cap", cc5_effective_threshold_cap },
    { "_tripped", cc5_tripped },
    { "_tail_soft_ceiling", cc5_tail_soft_ceiling },
    { "_render_tool_args_for_summary", cc5_render_tool_args_for_summary },
};
#define CC5_DISPATCH_N (sizeof(CC5_DISPATCH) / sizeof(CC5_DISPATCH[0]))

/* ---- line reader (\n only) ---- */
static char *cc5_h_readline(FILE *f)
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

static void cc5_h_run(const char *func, const json_t *in)
{
    cc5_script_t sc;
    sc.io = cc5_get(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();
    cc5_io_t io;
    io.call = cc5_h_dispatch;
    io.ctx = &sc;

    json_t *st = json_object();
    const json_t *sj = cc5_get(in, "state");
    if (cc5_is_obj(sj)) { json_free(st); st = json_copy(sj); }

    json_t *in_echo = json_copy(in);
    json_t *ret = NULL;
    int found = 0;
    for (size_t i = 0; i < CC5_DISPATCH_N; i++) {
        if (strcmp(CC5_DISPATCH[i].func, func) == 0) { ret = CC5_DISPATCH[i].fn(&io, in, st); found = 1; break; }
    }
    if (!found) ret = cc5_mkraise("RuntimeError", "unknown func");
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
    while ((line = cc5_h_readline(f)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec) continue;
        const char *func = cc5_str(cc5_get(rec, "func"));
        const json_t *in = cc5_get(rec, "in");
        if (func && in) cc5_h_run(func, in);
        json_free(rec);
    }
    if (f != stdin) fclose(f);
    return 0;
}
