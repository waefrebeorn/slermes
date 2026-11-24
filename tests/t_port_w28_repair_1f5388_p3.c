/* JSON-lines harness for the w28_repair_1f5388_p3 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w28_repair_1f5388_p3.h"

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
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, (int)sizeof line, stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = opt_str(in, "func");
        if (!func) func = "";
        if (!strcmp(func, "initial_report")) {
            out = w28_repair_1f5388_initial_report();
        } else if (!strcmp(func, "repair_skip")) {
            out = w28_repair_1f5388_repair_skip(json_obj_get(in, "report"),
                                                opt_bool(in, "has_exc"),
                                                opt_bool(in, "is_db_error"),
                                                opt_bool(in, "errorcode_present"),
                                                opt_num(in, "errorcode"),
                                                opt_str(in, "message"),
                                                opt_str(in, "error"));
        } else if (!strcmp(func, "run_strategies")) {
            out = w28_repair_1f5388_run_strategies(json_obj_get(in, "reasons"));
        } else if (!strcmp(func, "repair_flow")) {
            out = w28_repair_1f5388_repair_flow(opt_str(in, "scenario"),
                                                opt_str(in, "db_path"),
                                                opt_str(in, "profile_arg"));
        } else if (!strcmp(func, "preflight_skip")) {
            out = w28_repair_1f5388_preflight_skip(opt_str(in, "db_path"));
        } else if (!strcmp(func, "rw_or_vanished")) {
            out = w28_repair_1f5388_rw_or_vanished(opt_bool(in, "rw_first"),
                                                   opt_bool(in, "exists"),
                                                   opt_bool(in, "rw_second"));
        } else if (!strcmp(func, "preflight_error")) {
            out = w28_repair_1f5388_preflight_error(opt_str(in, "db_label"),
                                                    opt_str(in, "path"),
                                                    opt_bool(in, "is_dir"));
        } else if (!strcmp(func, "unlink_victims")) {
            out = w28_repair_1f5388_unlink_victims(opt_str(in, "db_path"));
        } else if (!strcmp(func, "strategies")) {
            out = w28_repair_1f5388_strategies();
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
