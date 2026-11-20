/* t_port_account_usage_nous_flow2.c -- harness for the FLOW port of the
 * UNCLAIMED remainder of agent/account_usage_nous.py in
 * src/agent/account_usage_nous_flow2.c (aun2_* entries).
 *
 * Reads JSON LINES from stdin (or argv[1]): {"func": <live_name>, "in": {...}}
 * where in.io carries the universal-seam scripts (seam-name -> constant or
 * per-call queue). Emits JSON LINES:
 *   {"func", "in", "ret", "trace", "st"}
 * ret is the projected return (or {"__raise__":kind,"message":str(exc)}),
 * trace the ordered seam events {ev,args,ret}, st the final state projection
 * ({} always here -- every ported function is pure).
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
#include "agent/account_usage_nous_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- universal-seam recording dispatcher ---- */
typedef struct {
    json_t *io;        /* borrowed in.io */
    json_t *cursors;   /* owned per-name queue cursors */
    json_t *trace;     /* owned ordered events */
} aun2_script_t;

/* drop None-valued object keys recursively; recurse into arrays so dict
 * elements are stripped too (matches the oracle norm_args). Null array
 * elements are kept. */
static json_t *aun2_h_strip_nulls(const json_t *v)
{
    if (!v) return json_null();
    if (v->type == JSON_ARRAY) {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++)
            json_append(a, aun2_h_strip_nulls(v->c.items[i]));
        return a;
    }
    if (v->type != JSON_OBJECT) return json_copy(v);
    json_t *o = json_object();
    for (size_t i = 0; i < v->c.count; i++) {
        json_t *val = v->c.items[i];
        if (val && val->type == JSON_NULL) continue;
        json_set(o, v->c.keys[i], aun2_h_strip_nulls(val));
    }
    return o;
}

static json_t *aun2_h_dispatch(void *ctx, const char *name, const json_t *args)
{
    aun2_script_t *sc = (aun2_script_t *)ctx;
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
    /* unwrap {"__script__":true,"ret":...} list-result wrapper */
    json_t *actual = result;
    if (!failclosed && actual && actual->type == JSON_OBJECT) {
        json_t *tag = json_obj_get(actual, "__script__");
        if (tag && tag->type == JSON_BOOL && tag->bool_val)
            actual = json_obj_get(actual, "ret");
    }
    json_t *retnode;
    if (failclosed) {
        char *m = aun2_xfmt("no script for %s", name);
        retnode = aun2_mkraise("RuntimeError", m ? m : "no script");
        free(m);
    } else {
        retnode = actual ? json_copy(actual) : json_null();
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", aun2_h_strip_nulls(args));
    json_set(ev, "ret", json_copy(retnode));
    json_append(sc->trace, ev);

    if (failclosed) return retnode;
    if (!actual || actual->type == JSON_NULL) { json_free(retnode); return NULL; }
    return retnode;
}

/* ---- entry dispatch ----
 * Cases carry the LIVE name. Module funcs drop the leading underscore;
 * CreditsView.<method> maps to aun2_cv_<method>. */
static json_t *aun2_h_entry(const char *func, aun2_io_t *io, const json_t *in, json_t *st)
{
    if (strncmp(func, "CreditsView.", 12) == 0) {
        const char *m = func + 12;
        if (strcmp(m, "__init__") == 0) return aun2_cv_init(io, in, st);
        if (strcmp(m, "__repr__") == 0) return aun2_cv_repr(io, in, st);
        if (strcmp(m, "__eq__") == 0) return aun2_cv_eq(io, in, st);
        if (strcmp(m, "__setattr__") == 0) return aun2_cv_setattr(io, in, st);
        if (strcmp(m, "__delattr__") == 0) return aun2_cv_delattr(io, in, st);
        return aun2_mkraise("RuntimeError", "unknown CreditsView method");
    }
    if (func[0] == '_') func++;
#define AUN2_DISPATCH(nm) if (strcmp(func, #nm) == 0) return aun2_##nm(io, in, st)
    AUN2_DISPATCH(fmt_usd);
    AUN2_DISPATCH(is_finite_num);
    AUN2_DISPATCH(build_nous_credits_snapshot);
    AUN2_DISPATCH(nous_credits_lines);
    AUN2_DISPATCH(snapshot_from_credits_state);
    AUN2_DISPATCH(build_credits_view);
#undef AUN2_DISPATCH
    return aun2_mkraise("RuntimeError", "unknown func");
}

/* ---- line reader (\n only; fixtures may carry raw U+2028 inside strings) ---- */
static char *aun2_h_readline(FILE *f)
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

static void aun2_h_run(const char *func, json_t *in)
{
    aun2_script_t sc;
    sc.io = aun2_get(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();
    aun2_io_t io;
    io.call = aun2_h_dispatch;
    io.ctx = &sc;
    json_t *st = json_object();
    json_t *in_echo = json_copy(in);
    json_t *ret = aun2_h_entry(func, &io, in, st);
    /* every ported function is pure: st stays {} on success and raise alike */
    if (aun2_is_raise(ret)) {
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
    while ((line = aun2_h_readline(f)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec) continue;
        const char *func = aun2_str(aun2_get(rec, "func"));
        json_t *in = aun2_get(rec, "in");
        if (func && in) aun2_h_run(func, in);
        json_free(rec);
    }
    if (f != stdin) fclose(f);
    return 0;
}