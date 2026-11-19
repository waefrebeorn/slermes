/* t_port_platform_registry_flow.c -- oracle harness for the FLOW port of
 * gateway/platform_registry.py (src/gateway/platforms/platform_registry_flow.c).
 *
 * Reads JSON LINES from stdin (or argv[1]): {"func": <live_name>, "in": {...}}
 * where in.io carries the universal-seam scripts (seam-name -> constant or
 * per-call queue) and in.state carries the mutable registry carrier. Emits:
 *   {"func", "in", "ret", "trace", "st"}
 * ret is the projected return OR a raise spec {"__raise__":kind,"message":...};
 * trace the ordered seam events {ev,args,ret}; st the final state window.
 *
 * Fail-closed: an absent / exhausted script returns a RuntimeError raise spec
 * ("no script for <name>"), recorded as the event ret BEFORE propagation. A bare
 * JSON array script is a per-call QUEUE; a list RESULT must be wrapped
 * {"__script__":true,"ret":[...]}. A scripted null returns a NULL pointer (None).
 *
 * Set-valued state (_cancelled_inflight / _failed_loads / _loading and each
 * _activation_env value) is sorted at echo on both sides. configured_load /
 * configured_entries iterate a set live in `any(wanted(n) ...)`; the trailing
 * consecutive `wanted` events are sorted by name here AND in the oracle (the
 * multi-name fixtures script wanted falsy for every name so any() exhausts the
 * set; truthy/raise wanted cases use single-name unresolved).
 *
 * Gate: bash tests/run_one_oracle.sh platform_registry_flow \
 *           oracle_platform_registry_flow.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gateway/platforms/platform_registry_flow.c"

/* ---- universal-seam recording dispatcher ---- */
typedef struct {
    const json_t *io;    /* borrowed in.io */
    json_t *cursors;     /* owned per-name queue cursors */
    json_t *trace;       /* owned ordered events */
} pr_script_t;

/* drop None-valued object keys recursively (array elements kept). */
static json_t *pr_h_strip_nulls(const json_t *v)
{
    if (!v) return json_null();
    if (v->type == JSON_OBJECT) {
        json_t *o = json_object();
        for (size_t i = 0; i < v->c.count; i++) {
            json_t *val = v->c.items[i];
            if (val && val->type == JSON_NULL) continue;
            json_set(o, v->c.keys[i], pr_h_strip_nulls(val));
        }
        return o;
    }
    if (v->type == JSON_ARRAY) {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++) json_append(a, pr_h_strip_nulls(v->c.items[i]));
        return a;
    }
    return json_copy(v);
}

static json_t *pr_h_dispatch(void *ctx, const char *name, const json_t *args)
{
    pr_script_t *sc = (pr_script_t *)ctx;
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
        char *m = pr_xfmt("no script for %s", name);
        retnode = pr_mkraise("RuntimeError", m ? m : "no script");
        free(m);
    } else {
        retnode = actual ? json_copy(actual) : json_null();
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", pr_h_strip_nulls(args));
    json_set(ev, "ret", json_copy(retnode));
    json_append(sc->trace, ev);

    if (failclosed) return retnode;
    if (!actual || actual->type == JSON_NULL) { json_free(retnode); return NULL; }
    return retnode;
}

/* ---- state echo normalization: sort set-valued keys ---- */
static void pr_h_sort_arr(json_t *arr)
{
    if (arr && arr->type == JSON_ARRAY && arr->c.count > 1)
        qsort(arr->c.items, arr->c.count, sizeof(json_t *), pr_str_cmp);
}

static json_t *pr_h_norm_st(const json_t *st)
{
    json_t *out = json_copy(st);
    const char *setkeys[] = { "_cancelled_inflight", "_failed_loads", "_loading" };
    for (int i = 0; i < 3; i++) pr_h_sort_arr(pr_get(out, setkeys[i]));
    json_t *ae = pr_get(out, "_activation_env");
    if (pr_is_obj(ae))
        for (size_t i = 0; i < ae->c.count; i++) pr_h_sort_arr(ae->c.items[i]);
    return out;
}

/* sort the maximal trailing run of events whose ev == name by args.name. */
static void pr_h_sort_trailing(json_t *trace, const char *name)
{
    if (!pr_is_arr(trace) || !name) return;
    size_t n = trace->c.count;
    size_t start = n;
    while (start > 0) {
        json_t *ev = trace->c.items[start - 1];
        const char *e = pr_str(pr_get(ev, "ev"));
        if (e && strcmp(e, name) == 0) start--;
        else break;
    }
    size_t count = n - start;
    if (count > 1) {
        /* insertion sort by args.name on the borrowed sub-range */
        for (size_t i = start + 1; i < n; i++) {
            json_t *key = trace->c.items[i];
            const char *kn = pr_str(pr_get(pr_get(key, "args"), "name"));
            size_t j = i;
            while (j > start) {
                json_t *prev = trace->c.items[j - 1];
                const char *pn = pr_str(pr_get(pr_get(prev, "args"), "name"));
                if (pn && kn && strcmp(pn, kn) > 0) {
                    trace->c.items[j] = trace->c.items[j - 1];
                    j--;
                } else break;
            }
            trace->c.items[j] = key;
        }
    }
}

/* ---- entry dispatch ---- */
typedef json_t *(*pr_fn_t)(pr_io_t *, const json_t *, json_t *);
typedef struct { const char *func; pr_fn_t fn; } pr_dispatch_t;
static const pr_dispatch_t PR_DISPATCH[] = {
    { "_plugin_scope_from_callable", pr_plugin_scope_from_callable },
    { "_caller_plugin_scope", pr_caller_plugin_scope },
    { "core_ships_platform", pr_core_ships_platform },
    { "__init__", pr_init },
    { "current_scope_key", pr_current_scope_key },
    { "_scope_maps", pr_scope_maps },
    { "_registration_state", pr_registration_state },
    { "_prune_scope", pr_prune_scope },
    { "note_activation_env", pr_note_activation_env },
    { "activation_env", pr_activation_env },
    { "snapshot_registration", pr_snapshot_registration },
    { "restore_registration", pr_restore_registration },
    { "_pending_names", pr_u_pending_names },
    { "pending_names", pr_pending_names },
    { "configured_entries", pr_configured_entries },
    { "configured_load", pr_configured_load },
    { "loaded_entries", pr_loaded_entries },
    { "forget_failed_loads", pr_forget_failed_loads },
    { "_load_pending", pr_load_pending },
    { "required_env_names", pr_required_env_names },
    { "deps_resolved_off_loop", pr_deps_resolved_off_loop },
    { "ensure_deps", pr_ensure_deps },
};
#define PR_DISPATCH_N (sizeof(PR_DISPATCH) / sizeof(PR_DISPATCH[0]))

/* ---- line reader (\n only) ---- */
static char *pr_h_readline(FILE *f)
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

static void pr_h_run(const char *func, const json_t *in)
{
    pr_script_t sc;
    sc.io = pr_get(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();
    pr_io_t io;
    io.call = pr_h_dispatch;
    io.ctx = &sc;

    json_t *st = json_object();
    const json_t *sj = pr_get(in, "state");
    if (pr_is_obj(sj)) { json_free(st); st = json_copy(sj); }

    json_t *in_echo = json_copy(in);
    json_t *ret = NULL;
    int found = 0;
    for (size_t i = 0; i < PR_DISPATCH_N; i++) {
        if (strcmp(PR_DISPATCH[i].func, func) == 0) { ret = PR_DISPATCH[i].fn(&io, in, st); found = 1; break; }
    }
    if (!found) ret = pr_mkraise("RuntimeError", "unknown func");
    else if (!ret) ret = json_null();

    /* normalize the set-iteration wanted tail for the configured flows. */
    if (strcmp(func, "configured_load") == 0 || strcmp(func, "configured_entries") == 0) {
        const char *wseam = pr_callable_seam(pr_get(in, "wanted"));
        pr_h_sort_trailing(sc.trace, wseam ? wseam : "wanted");
    }

    json_t *st_n = pr_h_norm_st(st);
    json_free(st);
    json_t *out = json_object();
    json_set(out, "func", json_string(func));
    json_set(out, "in", in_echo);
    json_set(out, "ret", ret);
    json_set(out, "trace", sc.trace);
    json_set(out, "st", st_n);
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
    while ((line = pr_h_readline(f)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec) continue;
        const char *func = pr_str(pr_get(rec, "func"));
        const json_t *in = pr_get(rec, "in");
        if (func && in) pr_h_run(func, in);
        json_free(rec);
    }
    if (f != stdin) fclose(f);
    return 0;
}
