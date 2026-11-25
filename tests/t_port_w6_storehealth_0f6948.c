/*
 * t_port_w6_storehealth_0f6948.c — Oracle harness for cron/store_health.py.
 * Reads JSONL fixture lines, emits {"func":...,"out":...,<echoed input keys>}.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_storehealth_0f6948.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        char *s = NULL;
        if (!strcmp(func, "probe_interval")) {
            out = json_number(w6_storehealth_0f6948_probe_interval());
        } else if (!strcmp(func, "fix_hint")) {
            s = w6_storehealth_0f6948_fix_hint(json_get_str(in, "store", NULL));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "describe_error")) {
            s = w6_storehealth_0f6948_describe_error(
                (int)json_get_num(in, "errnum", 0.0), json_get_str(in, "text", ""));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "unwritable_errno")) {
            out = json_bool(w6_storehealth_0f6948_unwritable_errno(
                (int)json_get_num(in, "errnum", 0.0)) ? 1 : 0);
        } else if (!strcmp(func, "outage_covers")) {
            out = json_bool(w6_storehealth_0f6948_outage_covers(
                json_obj_get(in, "records"), json_get_num(in, "due_at", 0.0),
                json_get_num(in, "grace", 0.0)) ? 1 : 0);
        } else if (!strcmp(func, "run_keys")) {
            out = w6_storehealth_0f6948_run_keys(json_obj_get(in, "jobs"));
        } else if (!strcmp(func, "skipped_runs")) {
            out = json_number((double)w6_storehealth_0f6948_skipped_runs(
                json_obj_get(in, "skipped")));
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
