/*
 * t_port_backup_flow3.c -- oracle harness for the 26 unclaimed backup.py
 * FLOW cores ported in src/cli/backup_flow3.c. Reads JSON-line cases from
 * stdin (tests/fixtures/backup_flow3.jsonl), wires the universal seam to the
 * scripted results in each case's "seams" object (a recording stub appending
 * one trace event per seam call, in call order), and emits
 *   {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}
 * FAIL-CLOSED: a missing seam key delivers
 * {"__raise__":"RuntimeError","msg":"unscripted seam: <name>"}; a present JSON
 * null is a valid None; a JSON array script is one result PER CALL (last
 * repeats, empty -> None); {"__arr__": v} is an explicit array VALUE.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli/backup_flow3.c"

typedef struct {
    const json_t *in;
    bk3_trace_t *trace;
    json_t *counters;
} bf3_ctx_t;

static json_t *bf3_args_drop_null(const json_t *args) {
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

static json_t *bf3_script_call(void *vctx, const char *name, const json_t *args) {
    bf3_ctx_t *ctx = vctx;
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (!sc) {
        char *msg = bk3_xsf("unscripted seam: %s", name);
        result = bk3_raise_spec("RuntimeError", msg);
        free(msg);
    } else if (sc->type == JSON_OBJECT && json_obj_get(sc, "__arr__")) {
        result = json_copy(json_obj_get(sc, "__arr__"));
    } else if (sc->type == JSON_ARRAY) {
        double idx = json_get_num(ctx->counters, name, 0);
        size_t i = (size_t)idx, n = json_len(sc);
        if (n > 0) result = json_copy(json_get(sc, i < n ? i : n - 1));
        json_set(ctx->counters, name, json_number(idx + 1));
    } else {
        result = json_copy(sc);
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", bf3_args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    bk3_trace_add(ctx->trace, e);
    return result;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    bk3_trace_t trace = {NULL};
    json_t *counters = json_object();
    bf3_ctx_t ctx = {in, &trace, counters};
    bk3_seams_t seams = {&ctx, bf3_script_call};
    const bk3_seams_t *sp = &seams;
    json_t *ret = NULL;

    if (!strcmp(func, "is_non_regular_path")) ret = bk3_is_non_regular_path(sp, in);
    else if (!strcmp(func, "is_link_path")) ret = bk3_is_link_path(sp, in);
    else if (!strcmp(func, "snapshot_db")) ret = bk3_snapshot_db(sp, in);
    else if (!strcmp(func, "discard_failed_zip_members")) ret = bk3_discard_failed_zip_members(sp, in);
    else if (!strcmp(func, "zip_sqlite_snapshot")) ret = bk3_zip_sqlite_snapshot(sp, in);
    else if (!strcmp(func, "write_zip_entries")) ret = bk3_write_zip_entries(sp, in);
    else if (!strcmp(func, "resolve_backup_output_path")) ret = bk3_resolve_backup_output_path(sp, in);
    else if (!strcmp(func, "collect_external_entries")) ret = bk3_collect_external_entries(sp, in);
    else if (!strcmp(func, "run_import")) ret = bk3_run_import(sp, in);
    else if (!strcmp(func, "import_members")) ret = bk3_import_members(sp, in);
    else if (!strcmp(func, "import_external_member")) ret = bk3_import_external_member(sp, in);
    else if (!strcmp(func, "import_members_exclusive")) ret = bk3_import_members_exclusive(sp, in);
    else if (!strcmp(func, "revive_gateway_after_import")) ret = bk3_revive_gateway_after_import(sp, in);
    else if (!strcmp(func, "newest_first")) ret = bk3_newest_first(sp, in);
    else if (!strcmp(func, "prune_oldest")) ret = bk3_prune_oldest(sp, in);
    else if (!strcmp(func, "create_prefixed_full_backup")) ret = bk3_create_prefixed_full_backup(sp, in);
    else if (!strcmp(func, "too_large")) ret = bk3_too_large(sp, in);
    else if (!strcmp(func, "is_trusted_root_auth_alias")) ret = bk3_is_trusted_root_auth_alias(sp, in);
    else if (!strcmp(func, "quick_snapshot_dir")) ret = bk3_quick_snapshot_dir(sp, in);
    else if (!strcmp(func, "restore_quick_snapshot_exclusive")) ret = bk3_restore_quick_snapshot_exclusive(sp, in);
    else if (!strcmp(func, "entry_paths")) ret = bk3_entry_paths(sp, in);
    else if (!strcmp(func, "restore_config_model_settings_if_rewritten")) ret = bk3_restore_config_model_settings_if_rewritten(sp, in);
    else if (!strcmp(func, "db_failure")) ret = bk3_db_failure(sp, in);
    else if (!strcmp(func, "capped_errors")) ret = bk3_capped_errors(sp, in);
    else if (!strcmp(func, "publish_path")) ret = bk3_publish_path(sp, in);
    else if (!strcmp(func, "progress")) ret = bk3_progress(sp, in);
    else fprintf(stderr, "unknown func: %s\n", func);

    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", trace.events ? json_copy(trace.events) : json_array());
    bk3_trace_free(&trace);
    json_free(counters);
}

int main(void) {
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    char *line = NULL; size_t cap = 0; ssize_t len;
    while ((len = getline(&line, &cap, stdin)) > 0) {
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        if (line[0] != '{') continue;
        char *err = NULL;
        json_t *c = json_parse(line, &err);
        if (err || !c || c->type != JSON_OBJECT) {
            fprintf(stderr, "bad case: %s\n", err ? err : "not object");
            free(err); json_free(c); continue;
        }
        const char *func = json_get_str(c, "func", "");
        const json_t *in = json_obj_get(c, "in");
        json_t *empty = json_object(), *out = json_object();
        run_case(func, in ? in : empty, out);
        json_free(empty);
        char *in_s = json_serialize(in), *out_s = json_serialize(out);
        char *func_s = json_escape_string(func);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", func_s,
               in_s ? in_s : "null", out_s ? out_s : "null");
        free(in_s); free(out_s); free(func_s);
        json_free(out); json_free(c);
    }
    free(line);
    return 0;
}
