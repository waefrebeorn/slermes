/* t_port_route_identity_flow.c -- harness for the FLOW + leaf port of the
 * unclaimed route-identity remainder (hermes_cli/route_identity.py x3 +
 * utils/urls.py x7) in src/cli/route_identity_flow.c.
 *
 * Reads JSON LINES from stdin (or argv[1]): {"func": <live_name>, "in": {...}}
 * where in.io carries the universal-seam scripts (seam-name -> constant or
 * per-call queue). Emits JSON LINES:
 *   {"func", "in", "ret", "trace", "st"}
 * ret is the projected return (or {"__raise__":kind,"message":str(exc)}),
 * trace the ordered seam events {ev,args,ret}, st the final state projection
 * ({} for every entry here -- all stateless -- and on every raise path,
 * matching the oracle's BaseException wrapper).
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
#include "cli/route_identity_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- universal-seam recording dispatcher ---- */
typedef struct {
    json_t *io;        /* borrowed in.io */
    json_t *cursors;   /* owned per-name queue cursors */
    json_t *trace;     /* owned ordered events */
} rid_script_t;

/* drop None-valued object keys recursively (array elements kept) */
static json_t *rid_h_strip_nulls(const json_t *v)
{
    if (!v) return json_null();
    if (v->type != JSON_OBJECT) return json_copy(v);
    json_t *o = json_object();
    for (size_t i = 0; i < v->c.count; i++) {
        json_t *val = v->c.items[i];
        if (val && val->type == JSON_NULL) continue;
        json_set(o, v->c.keys[i], rid_h_strip_nulls(val));
    }
    return o;
}

static json_t *rid_h_dispatch(void *ctx, const char *name, const json_t *args)
{
    rid_script_t *sc = (rid_script_t *)ctx;
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
        char *m = rid_xfmt("no script for %s", name);
        retnode = rid_mkraise("RuntimeError", m ? m : "no script");
        free(m);
    } else {
        retnode = actual ? json_copy(actual) : json_null();
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", rid_h_strip_nulls(args));
    json_set(ev, "ret", json_copy(retnode));
    json_append(sc->trace, ev);

    if (failclosed) return retnode;
    if (!actual || actual->type == JSON_NULL) { json_free(retnode); return NULL; }
    return retnode;
}

/* ---- entry dispatch ----
 * Cases carry the LIVE name (e.g. "_parse_base_url"); the C entry suffix
 * drops the leading underscore. Normalize before suffix dispatch. */
static json_t *rid_h_entry(const char *func, rid_io_t *io, const json_t *in, json_t *st)
{
    if (func[0] == '_') func++;
#define RID_DISPATCH(nm) if (strcmp(func, #nm) == 0) return rid_##nm(io, in, st)
    RID_DISPATCH(normalize_route_base_url);
    RID_DISPATCH(should_clear_context_pin);
    RID_DISPATCH(should_clear_context_pin_async);
    RID_DISPATCH(parse_base_url);
    RID_DISPATCH(hostname_of);
    RID_DISPATCH(base_url_hostname);
    RID_DISPATCH(base_url_path);
    RID_DISPATCH(base_url_origin);
    RID_DISPATCH(same_base_url_origin);
    RID_DISPATCH(base_url_host_matches);
#undef RID_DISPATCH
    return rid_mkraise("RuntimeError", "unknown func");
}

/* ---- line reader (\n only; fixtures may carry escaped control chars) ---- */
static char *rid_h_readline(FILE *f)
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

static void rid_h_run(const char *func, json_t *in)
{
    rid_script_t sc;
    sc.io = rid_get(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();
    rid_io_t io;
    io.call = rid_h_dispatch;
    io.ctx = &sc;
    json_t *st = json_object();
    json_t *in_echo = json_copy(in);
    json_t *ret = rid_h_entry(func, &io, in, st);
    /* raise paths echo {} (the oracle's BaseException wrapper does the same) */
    if (rid_is_raise(ret)) {
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
    while ((line = rid_h_readline(f)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec) continue;
        const char *func = rid_str(rid_get(rec, "func"));
        json_t *in = rid_get(rec, "in");
        if (func && in) rid_h_run(func, in);
        json_free(rec);
    }
    if (f != stdin) fclose(f);
    return 0;
}