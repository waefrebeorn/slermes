/* JSON-lines harness for the w28_repair_1f5388_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w28_repair_1f5388_p2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *opt_str(const json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static double opt_num(const json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}

static int opt_bool(const json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    if (!v) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    return 0;
}

int main(void) {
    static char line[1 << 20];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, (int)sizeof line, stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = opt_str(in, "func");
        if (!func) func = "";
        if (!strcmp(func, "integrity_structural")) {
            out = w28_repair_1f5388_integrity_structural(json_obj_get(in, "lines"),
                                                         json_obj_get(in, "master_rows"));
        } else if (!strcmp(func, "probe_reason")) {
            out = w28_repair_1f5388_probe_reason(json_obj_get(in, "rows"), opt_bool(in, "all_ok"));
        } else if (!strcmp(func, "strategy_statements")) {
            out = w28_repair_1f5388_strategy_statements(opt_str(in, "name"),
                                                        json_obj_get(in, "dupes"),
                                                        opt_num(in, "schema_version"));
        } else if (!strcmp(func, "probe_plan")) {
            out = w28_repair_1f5388_probe_plan();
        } else if (!strcmp(func, "schema_version_sql")) {
            out = w28_repair_1f5388_schema_version_sql(opt_num(in, "current"));
        } else if (!strcmp(func, "fingerprint_text")) {
            out = w28_repair_1f5388_fingerprint_text(opt_num(in, "size"),
                                                     opt_str(in, "digest_hex"));
        } else if (!strcmp(func, "ledger_name")) {
            out = w28_repair_1f5388_ledger_name(opt_str(in, "db_name"));
        } else if (!strcmp(func, "scratch_name")) {
            out = w28_repair_1f5388_scratch_name(opt_str(in, "db_name"));
        } else if (!strcmp(func, "staging_name")) {
            out = w28_repair_1f5388_staging_name(opt_str(in, "db_name"), opt_str(in, "stamp"));
        } else if (!strcmp(func, "backup_name")) {
            out = w28_repair_1f5388_backup_name(opt_str(in, "db_name"), opt_str(in, "stamp"),
                                                json_obj_get(in, "existing"));
        } else if (!strcmp(func, "existing_backups")) {
            out = w28_repair_1f5388_existing_backups(opt_str(in, "db_name"),
                                                     json_obj_get(in, "names"));
        } else if (!strcmp(func, "prune_victims")) {
            out = w28_repair_1f5388_prune_victims(opt_str(in, "db_name"),
                                                  json_obj_get(in, "names"),
                                                  opt_num(in, "keep"));
        } else if (!strcmp(func, "attempts_exhausted")) {
            out = w28_repair_1f5388_attempts_exhausted(json_obj_get(in, "recorded"),
                                                       json_obj_get(in, "fp"),
                                                       opt_num(in, "db_size"),
                                                       json_obj_get(in, "failed_attempts"));
        } else if (!strcmp(func, "next_attempts")) {
            out = w28_repair_1f5388_next_attempts(json_obj_get(in, "recorded"),
                                                  json_obj_get(in, "fp"),
                                                  json_obj_get(in, "failed_attempts"));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
