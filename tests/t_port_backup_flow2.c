/*
 * t_port_backup_flow2.c — oracle harness for the unclaimed backup.py FLOW
 * cores ported in src/cli/backup_flow2.c (snapshot_recovery_hint,
 * print_capped, import_skipped, import_member_rel, prune_prefixed_zips,
 * prune_incomplete_zips, write_zip_file, find_corrupt_members,
 * snapshot_epochs_restorable, and the three *_all_profiles orchestrators).
 *
 * Self-contained: includes the port .c by area-qualified name. Reads JSON-line
 * cases from stdin (tests/fixtures/backup_flow2.jsonl), wires the universal
 * seam to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 *
 * FAIL-CLOSED: a seam key MISSING from the script delivers
 * {"__raise__":"RuntimeError","msg":"unscripted seam: <name>"}; a present JSON
 * null is a valid None result; a JSON array script is one result PER CALL
 * (last repeats, empty -> None); {"__arr__": v} is an explicit array VALUE.
 * tests/oracle_backup_flow2.py drives the LIVE Python with the same scripts
 * and compares the ordered trace + return.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli/backup_flow2.c"

typedef struct {
    const json_t *in;
    bk2_trace_t *trace;
    json_t *counters;
} bf2_script_ctx_t;

/* Mirror of the oracle's seam(): null args are dropped so a scripted null and
 * an unscripted-but-nullable field compare identically on both sides. Args
 * and ret are SNAPSHOTS (json_copy): a later in-place state mutation cannot
 * leak backwards into an already recorded trace entry. */
static json_t *bf2_args_drop_null(const json_t *args) {
    json_t *out = json_object();
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++) {
            const json_t *v = args->c.items[i];
            if (v && v->type == JSON_NULL) continue;
            json_set(out, args->c.keys[i], json_copy(v));
        }
    }
    return out;
}

static json_t *bf2_script_call(void *vctx, const char *name, const json_t *args) {
    bf2_script_ctx_t *ctx = vctx;
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (!sc) {
        char *msg = bk2_xsf("unscripted seam: %s", name);
        result = bk2_raise_spec("RuntimeError", msg);
        free(msg);
    } else if (sc->type == JSON_OBJECT && json_obj_get(sc, "__arr__")) {
        result = json_copy(json_obj_get(sc, "__arr__"));
    } else if (sc->type == JSON_ARRAY) {
        double idx = json_get_num(ctx->counters, name, 0);
        size_t i = (size_t)idx;
        size_t n = json_len(sc);
        if (n > 0) result = json_copy(json_get(sc, i < n ? i : n - 1));
        json_set(ctx->counters, name, json_number(idx + 1));
    } else {
        result = json_copy(sc);
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", bf2_args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    bk2_trace_add(ctx->trace, e);
    return result;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    bk2_trace_t trace = {NULL};
    json_t *counters = json_object();
    bf2_script_ctx_t ctx = {in, &trace, counters};
    bk2_seams_t seams = {&ctx, bf2_script_call};
    const bk2_seams_t *sp = &seams;
    json_t *ret = NULL;

    if (!strcmp(func, "snapshot_recovery_hint")) {
        ret = bk2_snapshot_recovery_hint();
    } else if (!strcmp(func, "print_capped")) {
        ret = bk2_print_capped(sp, in);
    } else if (!strcmp(func, "import_skipped")) {
        ret = bk2_import_skipped(sp, bk2_jstr(json_obj_get(in, "rel")) ?: "");
    } else if (!strcmp(func, "import_member_rel")) {
        ret = bk2_import_member_rel(sp, in);
    } else if (!strcmp(func, "prune_prefixed_zips")) {
        ret = bk2_prune_prefixed_zips(sp, in);
    } else if (!strcmp(func, "prune_incomplete_zips")) {
        ret = bk2_prune_incomplete_zips(sp, in);
    } else if (!strcmp(func, "write_zip_file")) {
        ret = bk2_write_zip_file(sp, in);
    } else if (!strcmp(func, "find_corrupt_members")) {
        ret = bk2_find_corrupt_members(sp, in);
    } else if (!strcmp(func, "snapshot_epochs_restorable")) {
        ret = bk2_snapshot_epochs_restorable(sp, in);
    } else if (!strcmp(func, "create_pre_update_snapshots_all_profiles")) {
        ret = bk2_create_pre_update_snapshots_all_profiles(sp, in);
    } else if (!strcmp(func, "restore_config_model_settings_all_profiles")) {
        ret = bk2_restore_config_model_settings_all_profiles(sp, in);
    } else if (!strcmp(func, "restore_cron_jobs_all_profiles")) {
        ret = bk2_restore_cron_jobs_all_profiles(sp, in);
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", trace.events ? json_copy(trace.events) : json_array());
    bk2_trace_free(&trace);
    json_free(counters);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
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
        run_case(func, in ? in : empty, out);
        json_free(empty);
        char *in_s = json_serialize(in);
        char *out_s = json_serialize(out);
        char *func_s = json_escape_string(func);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", func_s,
               in_s ? in_s : "null", out_s ? out_s : "null");
        free(in_s); free(out_s); free(func_s);
        json_free(out);
        json_free(c);
    }
    free(line);
    return 0;
}
