/*
 * t_port_w6_cancellation_b52bf7.c — Oracle harness for cron/cancellation.py.
 * Reads JSONL fixture lines, emits {"func":...,"out":...,<echoed input keys>}.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_cancellation_b52bf7.h"

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
        if (!strcmp(func, "valid")) {
            out = json_bool(w6_cancellation_b52bf7_valid_execution_id(
                json_get_str(in, "id", NULL)) ? 1 : 0);
        } else if (!strcmp(func, "marker")) {
            s = w6_cancellation_b52bf7_marker_name(json_get_str(in, "id", NULL));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "cancel_reason")) {
            out = json_string(w6_cancellation_b52bf7_cancel_reason(
                json_get_str(in, "reason", NULL)));
        } else if (!strcmp(func, "cancel_run_none")) {
            out = w6_cancellation_b52bf7_cancel_run_none();
        } else if (!strcmp(func, "cancel_run")) {
            out = w6_cancellation_b52bf7_cancel_run(json_obj_get(in, "record"),
                json_get_str(in, "reason", NULL));
        } else if (!strcmp(func, "combined_is_set")) {
            out = json_bool(w6_cancellation_b52bf7_combined_is_set(
                json_obj_get(in, "events")) ? 1 : 0);
        } else if (!strcmp(func, "combined_reason")) {
            s = w6_cancellation_b52bf7_combined_reason(json_obj_get(in, "events"));
            out = s ? json_string(s) : json_null();
            free(s);
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
