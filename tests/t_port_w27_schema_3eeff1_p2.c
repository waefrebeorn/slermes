/* JSON-lines harness for the w27_schema_3eeff1_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_schema_3eeff1_p2.h"

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
        if (!strcmp(func, "misaligned_source")) {
            out = json_bool(w27_schema_3eeff1_fts_index_is_misaligned_source(json_obj_get(in, "table_sql")));
        } else if (!strcmp(func, "live_pk_columns")) {
            out = w27_schema_3eeff1_live_pk_columns(json_obj_get(in, "rows"),
                                                    json_get_bool(in, "missing", false));
        } else if (!strcmp(func, "rebuild_table_statements")) {
            out = w27_schema_3eeff1_rebuild_table_statements(
                opt_str(in, "table"), opt_str(in, "legacy_name"), opt_str(in, "ddl"),
                opt_str(in, "copy_sql"), json_obj_get(in, "indexes"));
        } else if (!strcmp(func, "rebuild_table")) {
            out = w27_schema_3eeff1_rebuild_table(
                opt_str(in, "table"), opt_str(in, "legacy_name"), opt_str(in, "ddl"),
                opt_str(in, "copy_sql"), json_obj_get(in, "indexes"),
                json_get_bool(in, "in_transaction", false),
                (int)json_get_num(in, "fail_at", -1.0));
        } else if (!strcmp(func, "rebuild_fts_indexes")) {
            out = w27_schema_3eeff1_rebuild_fts_indexes(json_get_bool(in, "legacy", false),
                                                        json_get_bool(in, "include_trigram", true));
        } else if (!strcmp(func, "drop_fts_triggers")) {
            out = w27_schema_3eeff1_drop_fts_triggers();
        } else if (!strcmp(func, "fts_triggers_missing")) {
            out = json_bool(w27_schema_3eeff1_fts_triggers_missing(
                json_obj_get(in, "names"), (int)json_get_num(in, "found", -1.0)));
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
