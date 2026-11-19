/* t_port_relay_metrics_flow.c — oracle harness for the UNCLAIMED third-wave
 * FLOW + leaf port of hermes_cli/observability/relay_shared_metrics.py
 * (src/cli/relay_metrics_flow.c).
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
 * Gate: bash tests/run_one_oracle.sh relay_metrics_flow \
 *           oracle_relay_metrics_flow.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli/relay_metrics_flow.c"

/* ---- universal-seam recording dispatcher ---- */
typedef struct {
    const json_t *io;    /* borrowed in.io */
    json_t *cursors;     /* owned per-name queue cursors */
    json_t *trace;       /* owned ordered events */
} rm_script_t;

/* drop None-valued object keys recursively (array elements kept) */
static json_t *rm_h_strip_nulls(const json_t *v)
{
    if (!v) return json_null();
    if (v->type != JSON_OBJECT) return json_copy(v);
    json_t *o = json_object();
    for (size_t i = 0; i < v->c.count; i++) {
        json_t *val = v->c.items[i];
        if (val && val->type == JSON_NULL) continue;
        json_set(o, v->c.keys[i], rm_h_strip_nulls(val));
    }
    return o;
}

static json_t *rm_h_dispatch(void *ctx, const char *name, const json_t *args)
{
    rm_script_t *sc = (rm_script_t *)ctx;
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
        char *m = rm_xfmt("no script for %s", name);
        retnode = rm_mkraise("RuntimeError", m ? m : "no script");
        free(m);
    } else {
        retnode = actual ? json_copy(actual) : json_null();
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", rm_h_strip_nulls(args));
    json_set(ev, "ret", json_copy(retnode));
    json_append(sc->trace, ev);

    if (failclosed) return retnode;
    if (!actual || actual->type == JSON_NULL) { json_free(retnode); return NULL; }
    return retnode;
}

/* ---- entry dispatch ---- */
typedef json_t *(*rm_fn_t)(rm_io_t *, const json_t *, json_t *);
typedef struct { const char *func; rm_fn_t fn; } rm_dispatch_t;
static const rm_dispatch_t RM_DISPATCH[] = {
    { "_run_scoped", rm_run_scoped },
    { "_mark", rm_mark },
    { "_emit_install_snapshot_if_due", rm_emit_install_snapshot_if_due },
    { "_owner_scope", rm_owner_scope },
    { "_flush_and_export", rm_flush_and_export },
    { "_run_flush_worker", rm_run_flush_worker },
    { "_schedule_flush_and_export", rm_schedule_flush_and_export },
    { "_close_and_export", rm_close_and_export },
    { "_share_spent", rm_share_spent },
    { "_with_route_run", rm_with_route_run },
    { "_close_segment", rm_close_segment },
    { "_emit_summary_rows", rm_emit_summary_rows },
    { "_close_unseen_segment", rm_close_unseen_segment },
    { "_flush_lineage", rm_flush_lineage },
    { "_send_exported_packages", rm_send_exported_packages },
    { "_run_send_pass", rm_run_send_pass },
    { "record_auxiliary_tokens", rm_record_auxiliary_tokens },
    { "record_switch_after", rm_record_switch_after },
    { "record_friction", rm_record_friction },
    { "_reconcile_store_consent", rm_reconcile_store_consent },
    { "_reconcile_send_consent_once", rm_reconcile_send_consent_once },
    { "record_session_friction", rm_record_session_friction },
    { "record_session_tools", rm_record_session_tools },
    { "record_known_cache_break", rm_record_known_cache_break },
    { "record_model_switch_after", rm_record_model_switch_after },
};
#define RM_DISPATCH_N (sizeof(RM_DISPATCH) / sizeof(RM_DISPATCH[0]))

/* ---- line reader (\n only) ---- */
static char *rm_h_readline(FILE *f)
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

static void rm_h_run(const char *func, const json_t *in)
{
    rm_script_t sc;
    sc.io = rm_get(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();
    rm_io_t io;
    io.call = rm_h_dispatch;
    io.ctx = &sc;

    json_t *st = json_object();
    const json_t *sj = rm_get(in, "state");
    if (rm_is_obj(sj)) { json_free(st); st = json_copy(sj); }

    json_t *in_echo = json_copy(in);
    json_t *ret = NULL;
    int found = 0;
    for (size_t i = 0; i < RM_DISPATCH_N; i++) {
        if (strcmp(RM_DISPATCH[i].func, func) == 0) { ret = RM_DISPATCH[i].fn(&io, in, st); found = 1; break; }
    }
    if (!found) ret = rm_mkraise("RuntimeError", "unknown func");
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
    while ((line = rm_h_readline(f)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec) continue;
        const char *func = rm_str(rm_get(rec, "func"));
        const json_t *in = rm_get(rec, "in");
        if (func && in) rm_h_run(func, in);
        json_free(rec);
    }
    if (f != stdin) fclose(f);
    return 0;
}