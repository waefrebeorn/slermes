/* JSON-lines harness for the w20_hostedroom_b5adc0 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_hostedroom_b5adc0.h"

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
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "source_path")) {
            out = w20_hostedroom_b5adc0_source_path(json_get_str(in, "db_path", NULL));
        } else if (!strcmp(func, "settled")) {
            out = w20_hostedroom_b5adc0_settled(json_get_str(in, "db_path", NULL),
                                                json_get_bool(in, "source_failed", false) ? 1 : 0,
                                                json_get_bool(in, "table_exists", false) ? 1 : 0,
                                                json_get_bool(in, "marker_row", false) ? 1 : 0);
        } else if (!strcmp(func, "select_expressions")) {
            out = w20_hostedroom_b5adc0_select_expressions(json_get_str(in, "name", ""),
                                                           json_obj_get(in, "source_cols"),
                                                           json_obj_get(in, "target_cols"),
                                                           json_obj_get(in, "defaults"));
        } else if (!strcmp(func, "plan_tables")) {
            out = w20_hostedroom_b5adc0_plan_tables(json_obj_get(in, "names"));
        } else if (!strcmp(func, "table_included")) {
            out = json_bool(w20_hostedroom_b5adc0_table_included(json_get_str(in, "name", "")) ? 1 : 0);
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
