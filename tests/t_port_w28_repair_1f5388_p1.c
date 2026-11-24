/* JSON-lines harness for the w28_repair_1f5388_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w28_repair_1f5388_p1.h"

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

/* The sizes _repair_snapshot_timeout_seconds() stats: the main file, then each
 * PRESENT sidecar (a sidecar that vanishes mid-walk contributes nothing). */
static json_t *sizes_from_request(const json_t *in) {
    json_t *arr = json_array();
    json_t *list = json_obj_get(in, "sidecar_bytes");
    json_append(arr, json_number(opt_num(in, "db_bytes")));
    for (size_t i = 0; list && i < json_len(list); i++)
        json_append(arr, json_copy(json_get(list, i)));
    return arr;
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
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, (int)sizeof line, stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = opt_str(in, "func");
        if (!func) func = "";
        if (!strcmp(func, "constants")) {
            out = w28_repair_1f5388_constants();
        } else if (!strcmp(func, "sidecars")) {
            out = w28_repair_1f5388_sidecars(opt_str(in, "db_path"));
        } else if (!strcmp(func, "mask_header")) {
            out = w28_repair_1f5388_mask_header(opt_str(in, "head_hex"));
        } else if (!strcmp(func, "headroom")) {
            out = w28_repair_1f5388_headroom(opt_num(in, "total_bytes"));
        } else if (!strcmp(func, "snapshot_timeout")) {
            json_t *sizes = sizes_from_request(in);
            out = w28_repair_1f5388_snapshot_timeout(sizes);
            json_free(sizes);
        } else if (!strcmp(func, "disk_budget_error")) {
            out = w28_repair_1f5388_disk_budget_error(opt_str(in, "parent"), opt_str(in, "exc"),
                                                      opt_str(in, "refusal"));
        } else if (!strcmp(func, "scratch_space_error")) {
            out = w28_repair_1f5388_scratch_space_error(opt_str(in, "parent"),
                                                        opt_num(in, "snapshot_bytes"),
                                                        opt_num(in, "free_bytes"),
                                                        opt_num(in, "headroom_bytes"));
        } else if (!strcmp(func, "backup_free_space_error")) {
            out = w28_repair_1f5388_backup_free_space_error(opt_str(in, "parent"),
                                                            opt_num(in, "need_bytes"),
                                                            opt_num(in, "free_bytes"),
                                                            opt_num(in, "headroom_bytes"),
                                                            opt_str(in, "db_path"));
        } else if (!strcmp(func, "manual_hint")) {
            out = w28_repair_1f5388_manual_hint(opt_str(in, "db_path"));
        } else if (!strcmp(func, "failure_consumes_attempt")) {
            out = w28_repair_1f5388_failure_consumes_attempt(opt_bool(in, "is_db_error"),
                                                             opt_bool(in, "errorcode_present"),
                                                             opt_num(in, "errorcode"),
                                                             opt_str(in, "message"));
        } else if (!strcmp(func, "temporary_file_failure")) {
            out = w28_repair_1f5388_temporary_file_failure(opt_bool(in, "errorcode_present"),
                                                           opt_num(in, "errorcode"),
                                                           opt_str(in, "message"));
        } else if (!strcmp(func, "schema_not_built")) {
            out = w28_repair_1f5388_schema_not_built(opt_str(in, "message"));
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
        json_free(out);
        json_free(in);
    }
    return 0;
}
