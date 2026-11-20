/* t_port_observability_metrics_flow.c — oracle harness for the UNCLAIMED
 * hermes_cli/observability/shared_metrics_fields.py FLOW + leaf port
 * (src/cli/observability_metrics_flow.c). The task-named metrics.py does not
 * exist; shared_metrics_fields.py is the real defining module.
 *
 * Reads JSON LINES from stdin (or argv[1]): {"func": <live_name>, "in": {...}}
 * where in.io carries the universal-seam scripts (seam-name -> constant or
 * per-call queue). Emits JSON LINES:
 *   {"func", "in", "ret", "trace", "st"}
 * ret is the projected return OR a raise spec {"__raise__":kind,"message":...};
 * trace the ordered seam events {ev,args,ret}; st the final state window ({} —
 * the field builders are pure).
 *
 * Fail-closed: an absent / exhausted script returns a RuntimeError raise spec
 * ("no script for <name>"), recorded as the event ret BEFORE propagation. A
 * bare JSON array script is a per-call QUEUE; a list RESULT must be wrapped
 * {"__script__":true,"ret":[...]}. A scripted null returns a NULL pointer
 * (None), not a json_null node.
 *
 * Gate: bash tests/run_one_oracle.sh observability_metrics_flow \
 *           oracle_observability_metrics_flow.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli/observability_metrics_flow.c"

/* ---- universal-seam recording dispatcher ---- */
typedef struct {
    const json_t *io;    /* borrowed in.io */
    json_t *cursors;     /* owned per-name queue cursors */
    json_t *trace;       /* owned ordered events */
} om_script_t;

/* drop None-valued object keys recursively (array elements kept) */
static json_t *om_h_strip_nulls(const json_t *v)
{
    if (!v) return json_null();
    if (v->type != JSON_OBJECT) return json_copy(v);
    json_t *o = json_object();
    for (size_t i = 0; i < v->c.count; i++) {
        json_t *val = v->c.items[i];
        if (val && val->type == JSON_NULL) continue;
        json_set(o, v->c.keys[i], om_h_strip_nulls(val));
    }
    return o;
}

static json_t *om_h_dispatch(void *ctx, const char *name, const json_t *args)
{
    om_script_t *sc = (om_script_t *)ctx;
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
        char *m = om_xfmt("no script for %s", name);
        retnode = om_mkraise("RuntimeError", m ? m : "no script");
        free(m);
    } else {
        retnode = actual ? json_copy(actual) : json_null();
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", om_h_strip_nulls(args));
    json_set(ev, "ret", json_copy(retnode));
    json_append(sc->trace, ev);

    if (failclosed) return retnode;
    if (!actual || actual->type == JSON_NULL) { json_free(retnode); return NULL; }
    return retnode;
}

/* ---- entry dispatch ---- */
typedef json_t *(*om_fn_t)(om_io_t *, const json_t *, json_t *);
typedef struct { const char *func; om_fn_t fn; } om_dispatch_t;
static const om_dispatch_t OM_DISPATCH[] = {
    { "_number", om_number },
    { "provider_identifier", om_provider_identifier },
    { "session_fields", om_session_fields },
    { "install_age_bucket", om_install_age_bucket },
    { "ttft_bucket", om_ttft_bucket },
    { "context_fill_bucket", om_context_fill_bucket },
    { "model_token_fields", om_model_token_fields },
    { "setup_completed_fields", om_setup_completed_fields },
    { "compression_failure_class", om_compression_failure_class },
    { "compression_fields", om_compression_fields },
    { "model_switch_fields", om_model_switch_fields },
    { "fallback_fields", om_fallback_fields },
    { "slash_command_fields", om_slash_command_fields },
    { "extension_install_fields", om_extension_install_fields },
    { "tagged_failure_class", om_tagged_failure_class },
    { "exception_failure_class", om_exception_failure_class },
    { "extension_failure_class", om_extension_failure_class },
    { "extension_registry", om_extension_registry },
    { "_long_session", om_long_session },
    { "milestones_for", om_milestones_for },
};
#define OM_DISPATCH_N (sizeof(OM_DISPATCH) / sizeof(OM_DISPATCH[0]))

/* ---- line reader (\n only) ---- */
static char *om_h_readline(FILE *f)
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

static void om_h_run(const char *func, const json_t *in)
{
    om_script_t sc;
    sc.io = om_get(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();
    om_io_t io;
    io.call = om_h_dispatch;
    io.ctx = &sc;

    json_t *st = json_object();   /* pure module: empty state window */

    json_t *in_echo = json_copy(in);
    json_t *ret = NULL;
    int found = 0;
    for (size_t i = 0; i < OM_DISPATCH_N; i++) {
        if (strcmp(OM_DISPATCH[i].func, func) == 0) { ret = OM_DISPATCH[i].fn(&io, in, st); found = 1; break; }
    }
    if (!found) ret = om_mkraise("RuntimeError", "unknown func");
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
    while ((line = om_h_readline(f)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec) continue;
        const char *func = om_str(om_get(rec, "func"));
        const json_t *in = om_get(rec, "in");
        if (func && in) om_h_run(func, in);
        json_free(rec);
    }
    if (f != stdin) fclose(f);
    return 0;
}