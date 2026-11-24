/* JSON-lines harness for the w20_hostedroom_d4f060 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_hostedroom_d4f060.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *opt_str(json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    if (!v || v->type != JSON_STRING) return NULL;
    return v->str_val;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "source_path")) {
            char *p = w20_hostedroom_d4f060_source_path(opt_str(in, "db_path"));
            out = p ? json_string(p) : json_null();
            free(p);
        } else if (!strcmp(func, "settled")) {
            out = json_bool(w20_hostedroom_d4f060_settled(
                opt_str(in, "db_path"),
                json_get_bool(in, "in_failed", false),
                json_get_bool(in, "table_exists", false),
                json_get_bool(in, "marker_row", false)) ? 1 : 0);
        } else if (!strcmp(func, "select_expressions")) {
            out = w20_hostedroom_d4f060_select_expressions(
                opt_str(in, "name"),
                json_obj_get(in, "source_columns"),
                json_obj_get(in, "target_columns"),
                json_obj_get(in, "legacy_columns"));
        } else if (!strcmp(func, "table_skipped")) {
            out = json_bool(w20_hostedroom_d4f060_table_skipped(opt_str(in, "name")) ? 1 : 0);
        } else if (!strcmp(func, "ordered_tables")) {
            out = w20_hostedroom_d4f060_ordered_tables(json_obj_get(in, "names"));
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
