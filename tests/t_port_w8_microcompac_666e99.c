/* t_port_w8_microcompac_666e99.c — harness for agent/micro_compaction.py port. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_microcompac_666e99.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "summary_marker")) {
            out = json_bool(w8_microcompac_666e99_is_summary_marker(in) ? true : false);
        } else if (!strcmp(func, "micro_marker")) {
            out = json_bool(w8_microcompac_666e99_is_micro_marker(in) ? true : false);
        } else if (!strcmp(func, "rolling")) {
            out = w8_microcompac_666e99_rolling_summary_from_marker(in);
        } else if (!strcmp(func, "render")) {
            out = w8_microcompac_666e99_render_micro_marker_content(in);
        } else if (!strcmp(func, "record_failure")) {
            out = w8_microcompac_666e99_record_micro_failure(in);
        } else if (!strcmp(func, "reset_failure")) {
            out = w8_microcompac_666e99_reset_micro_failure_tracking(json_obj_get(in, "state"));
        } else if (!strcmp(func, "find_exchange")) {
            out = w8_microcompac_666e99_find_one_exchange(in);
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
