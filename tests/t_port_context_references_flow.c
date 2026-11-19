/* t_port_context_references_flow.c -- oracle harness for the FLOW + leaf port
 * of the UNCLAIMED remainder of agent/context_references.py
 * (src/agent/context_references_flow.c).
 *
 * Reads JSON LINES from stdin (or argv[1]): {"func": <live_name>, "in": {...}}
 * where in.io carries the universal-seam scripts (seam-name -> constant or
 * per-call queue) and in.state carries the mutable carrier for the in-place
 * functions. Emits JSON LINES:
 *   {"func", "in", "ret", "trace", "st"}
 * ret is the projected return OR a raise spec {"__raise__":kind,"message":...};
 * trace the ordered seam events {ev,args,ret}; st the final state window
 * (emptied on a raise exit, mirroring the oracle's run_case).
 *
 * Fail-closed: an absent / exhausted script returns a RuntimeError raise spec
 * ("no script for <name>"), recorded as the event ret BEFORE propagation. A
 * bare JSON array script is a per-call QUEUE; a list RESULT must be wrapped
 * {"__script__":true,"ret":[...]}. A scripted null returns a NULL pointer
 * (None), not a json_null node.
 *
 * Gate: bash tests/run_one_oracle.sh context_references_flow \
 *           oracle_context_references_flow.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agent/context_references_flow.c"

/* ---- universal-seam recording dispatcher ---- */
typedef struct {
    const json_t *io;    /* borrowed in.io */
    json_t *cursors;     /* owned per-name queue cursors */
    json_t *trace;       /* owned ordered events */
} cr_script_t;

/* drop None-valued object keys recursively (array elements kept) */
static json_t *cr_h_strip_nulls(const json_t *v)
{
    if (!v) return json_null();
    if (v->type != JSON_OBJECT) return json_copy(v);
    json_t *o = json_object();
    for (size_t i = 0; i < v->c.count; i++) {
        json_t *val = v->c.items[i];
        if (val && val->type == JSON_NULL) continue;
        json_set(o, v->c.keys[i], cr_h_strip_nulls(val));
    }
    return o;
}

static json_t *cr_h_dispatch(void *ctx, const char *name, const json_t *args)
{
    cr_script_t *sc = (cr_script_t *)ctx;
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
        char *m = cr_xfmt("no script for %s", name);
        retnode = cr_mkraise("RuntimeError", m ? m : "no script");
        free(m);
    } else {
        retnode = actual ? json_copy(actual) : json_null();
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", cr_h_strip_nulls(args));
    json_set(ev, "ret", json_copy(retnode));
    json_append(sc->trace, ev);

    if (failclosed) return retnode;
    if (!actual || actual->type == JSON_NULL) { json_free(retnode); return NULL; }
    return retnode;
}

/* ---- entry dispatch ---- */
typedef json_t *(*cr_fn_t)(cr_io_t *, const json_t *, json_t *);
typedef struct { const char *func; cr_fn_t fn; } cr_dispatch_t;
static const cr_dispatch_t CR_DISPATCH[] = {
    { "__post_init__", cr_post_init },
    { "autocomplete", cr_autocomplete },
    { "expand", cr_expand },
    { "publish", cr_publish },
    { "git_args_diff", cr_git_args_diff },
    { "git_args_staged", cr_git_args_staged },
    { "git_args_git", cr_git_args_git },
    { "_expand_path_reference", cr_expand_path_reference },
    { "_read_file_reference", cr_read_file_reference },
    { "_next_line", cr_next_line },
    { "_format_file_reference", cr_format_file_reference },
    { "_run_quiet", cr_run_quiet },
    { "_drain", cr_drain },
    { "_is_under", cr_is_under },
    { "_composer_paste_roots", cr_composer_paste_roots },
    { "_agent_staged_path", cr_agent_staged_path },
    { "_on_disk_reference_block", cr_on_disk_reference_block },
    { "_oversized_text_reference_block", cr_oversized_text_reference_block },
};
#define CR_DISPATCH_N (sizeof(CR_DISPATCH) / sizeof(CR_DISPATCH[0]))

/* ---- line reader (\n only) ---- */
static char *cr_h_readline(FILE *f)
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

static void cr_h_run(const char *func, const json_t *in)
{
    cr_script_t sc;
    sc.io = cr_get(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();
    cr_io_t io;
    io.call = cr_h_dispatch;
    io.ctx = &sc;

    json_t *st = json_object();
    const json_t *sj = cr_get(in, "state");
    if (cr_is_obj(sj)) { json_free(st); st = json_copy(sj); }

    json_t *in_echo = json_copy(in);
    json_t *ret = NULL;
    int found = 0;
    for (size_t i = 0; i < CR_DISPATCH_N; i++) {
        if (strcmp(CR_DISPATCH[i].func, func) == 0) { ret = CR_DISPATCH[i].fn(&io, in, st); found = 1; break; }
    }
    if (!found) ret = cr_mkraise("RuntimeError", "unknown func");
    else if (!ret) ret = json_null();

    /* a raise exit discards the state window (oracle run_case: st={}) */
    if (cr_is_raise(ret)) { json_free(st); st = json_object(); }

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
    while ((line = cr_h_readline(f)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec) continue;
        const char *func = cr_str(cr_get(rec, "func"));
        const json_t *in = cr_get(rec, "in");
        if (func && in) cr_h_run(func, in);
        json_free(rec);
    }
    if (f != stdin) fclose(f);
    return 0;
}