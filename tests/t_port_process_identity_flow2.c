/*
 * t_port_process_identity_flow2.c — oracle harness for the hermes_cli/process_identity.py
 * UNCLAIMED remainder ported in src/cli/process_identity_flow2.c.
 *
 * Self-contained: includes the port .c by area-qualified path. Reads JSON-line cases
 * from stdin (tests/fixtures/process_identity_flow2.jsonl), wires the universal seam to
 * the SCRIPTED results in each case's "seams" object (a recording stub appending one
 * trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...]}} (+ "state" for live_boot_id).
 *
 * FAIL-CLOSED script semantics (identical to the oracle): a MISSING script returns a
 * RuntimeError raise spec {"__raise__":"RuntimeError","msg":"no script for seam: <name>"};
 * a scripted JSON null returns a NULL pointer (None), never a json_null node; an ARRAY
 * script is a per-call queue (last value repeats, empty array -> None);
 * {"__raise__": cls, "msg": ...} scripts an exception. The observable effects are the
 * ordered seam trace + the return value (+ the live_boot_id memo state).
 * tests/oracle_process_identity_flow2.py drives the LIVE Python with the same scripts
 * and compares ordered trace + ret (+ state).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/process_identity_flow2.c"

typedef struct {
    const json_t *in;
    json_t *trace;
    json_t *counters;
} script_ctx_t;

static json_t *args_drop_null(const json_t *args) {
    json_t *out = json_object();
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++) {
            const json_t *v = args->c.items[i];
            if (!v || v->type == JSON_NULL) continue;
            json_set(out, args->c.keys[i], json_copy(v));
        }
    }
    return out;
}

static json_t *script_call(void *vctx, const char *name, const json_t *args) {
    script_ctx_t *ctx = vctx;
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (!sc) {
        /* FAIL-CLOSED: a missing script is a RuntimeError raise spec (NOT None). */
        char *m = pi2_xsf("no script for seam: %s", name);
        result = pi2_raise("RuntimeError", m);
        free(m);
    } else if (sc->type == JSON_ARRAY) {
        double idx = json_get_num(ctx->counters, name, 0);
        size_t i = (size_t)idx;
        size_t n = json_len(sc);
        if (n > 0) result = json_copy(json_get(sc, i < n ? i : n - 1));
        json_set(ctx->counters, name, json_number(idx + 1));
    } else {
        result = json_copy(sc);
    }
    /* Contract: NULL == None. A scripted null must NOT surface as a non-NULL
     * json_null node (truthiness / is-not-None guards would read it wrong). */
    if (result && result->type == JSON_NULL) {
        json_free(result);
        result = NULL;
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args_drop_null(args));   /* borrowed args: copy, do not free */
    json_set(e, "ret", result ? json_copy(result) : json_null());
    if (!ctx->trace) ctx->trace = json_array();
    json_append(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define J(k) json_obj_get(in, k)

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = {in, NULL, json_object()};
    pi2_seams_t seams = {&ctx, script_call};
    const pi2_seams_t *sp = &seams;
    json_t *r = NULL;

    if (!strcmp(func, "boot_start")) {
        r = pi2_boot_start(sp, J("proc"), J("psutil"));
    } else if (!strcmp(func, "process_create_time")) {
        r = pi2_process_create_time(sp, J("pid"));
    } else if (!strcmp(func, "live_boot_id")) {
        const json_t *st = J("state");
        json_t *state = (st && st->type == JSON_OBJECT) ? json_copy(st) : json_object();
        int calls = (int)json_get_num(in, "calls", 1);
        if (calls < 1) calls = 1;
        for (int i = 0; i < calls; i++) {
            json_free(r);
            r = pi2_live_boot_id(sp, state);
        }
        json_set(out, "state", state);
    } else if (!strcmp(func, "entry_boot")) {
        r = pi2_entry_boot(sp, J("entry"), S("key"));
    } else if (!strcmp(func, "verified_boot_start")) {
        r = pi2_verified_boot_start(sp, J("pid"), J("create_time"), J("psutil"));
    } else if (!strcmp(func, "same_incarnation")) {
        r = pi2_same_incarnation(sp, J("proc"), J("create_time"), J("boot"), J("psutil"));
    } else if (!strcmp(func, "spawned_by_this_process")) {
        r = pi2_spawned_by_this_process(sp, J("pid"));
    } else if (!strcmp(func, "read_ledger_or_quarantine")) {
        r = pi2_read_ledger_or_quarantine(sp, S("path"));
    } else if (!strcmp(func, "is_desktop_owned_backend")) {
        r = pi2_is_desktop_owned_backend(sp, J("env"), J("sys_argv"), J("argv"));
    } else if (!strcmp(func, "local_desktop_client")) {
        r = pi2_local_desktop_client(J("identity"), J("declared"));
    } else if (!strcmp(func, "desktop_spawner_identity")) {
        r = pi2_desktop_spawner_identity(J("env"));
    } else if (!strcmp(func, "new_entry")) {
        r = pi2_new_entry(sp, J("pid"), J("create_time"), S("purpose"), J("project_root"),
                          J("spawner_pid"), J("spawner_create"), J("psutil"));
    } else if (!strcmp(func, "entry_still_running")) {
        r = pi2_entry_still_running(sp, J("entry"));
    } else if (!strcmp(func, "is_owner_death_supervisor")) {
        r = pi2_is_owner_death_supervisor(J("argv"), S("script_path"));
    } else if (!strcmp(func, "terminate_then_kill")) {
        r = pi2_terminate_then_kill(sp, J("pid"), J("create_time"), J("boot"), J("psutil"));
    } else if (!strcmp(func, "reparented_orphan")) {
        r = pi2_reparented_orphan(sp, J("pid"), J("reap_min_age"));
    } else if (!strcmp(func, "reap_orphaned_backend_owner")) {
        r = pi2_reap_orphaned_backend_owner(sp, J("pid"), J("create_time"), J("kill_fn"),
                                            J("psutil"), J("reap_min_age"));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    json_set(out, "ret", r ? r : json_null());
    json_set(out, "trace", ctx.trace ? ctx.trace : json_array());
    json_free(ctx.counters);
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    while ((len = getline(&line, &cap, stdin)) > 0) {
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        if (line[0] != '{') continue;
        char *err = NULL;
        json_t *c = json_parse(line, &err);
        if (err || !c || c->type != JSON_OBJECT) {
            fprintf(stderr, "bad case: %s\n", err ? err : "not object");
            free(err);
            json_free(c);
            continue;
        }
        const char *func = json_get_str(c, "func", "");
        const json_t *in = json_obj_get(c, "in");
        json_t *empty = json_object();
        json_t *out = json_object();
        /* Reserve envelope key positions: the oracle's want is built {ret, trace}
         * (+ state for live_boot_id) and canon() treats insertion order as semantic. */
        json_set(out, "ret", json_null());
        json_set(out, "trace", json_array());
        run_case(func, in ? in : empty, out);
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
    free(line);
    return 0;
}