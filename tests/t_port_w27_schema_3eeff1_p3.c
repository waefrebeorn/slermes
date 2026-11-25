/* JSON-lines harness for the w27_schema_3eeff1_p3 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_schema_3eeff1_p3.h"

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
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, (int)sizeof line, stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = opt_str(in, "func");
        if (!func) func = "";
        if (!strcmp(func, "is_trigram_unavailable_error")) {
            out = json_bool(w27_schema_3eeff1_is_trigram_unavailable_error(opt_str(in, "message")));
        } else if (!strcmp(func, "is_fts5_unavailable_error")) {
            out = json_bool(w27_schema_3eeff1_is_fts5_unavailable_error(opt_str(in, "message")));
        } else if (!strcmp(func, "db_has_legacy_inline_fts")) {
            out = json_bool(w27_schema_3eeff1_db_has_legacy_inline_fts(json_obj_get(in, "table_sql")));
        } else if (!strcmp(func, "db_has_trigram_tool_calls_projection")) {
            out = json_bool(w27_schema_3eeff1_db_has_trigram_tool_calls_projection(json_obj_get(in, "table_sql")));
        } else if (!strcmp(func, "retired_cjk_index_present")) {
            out = json_bool(w27_schema_3eeff1_retired_cjk_index_present(json_obj_get(in, "names")));
        } else if (!strcmp(func, "title_rank")) {
            out = json_number(w27_schema_3eeff1_title_rank(opt_str(in, "source")));
        } else if (!strcmp(func, "has_fts_trash")) {
            out = json_bool(w27_schema_3eeff1_has_fts_trash(json_obj_get(in, "tables")));
        } else if (!strcmp(func, "next_title_in_lineage")) {
            char *s = w27_schema_3eeff1_next_title_in_lineage(opt_str(in, "base"),
                                                              json_obj_get(in, "titles"));
            out = json_string(s);
            free(s);
        } else if (!strcmp(func, "constants")) {
            json_t *o = json_object();
            json_set(o, "title_source_derived", json_string("derived"));
            json_set(o, "title_source_llm", json_string("llm"));
            json_set(o, "title_source_user", json_string("user"));
            json_t *objs = json_array();
            const char *r[10] = {
                "messages_fts_cjk_insert", "messages_fts_cjk_delete", "messages_fts_cjk_update",
                "messages_fts_cjk_src", "messages_fts_cjk",
                "messages_fts_cjk_content", "messages_fts_cjk_data", "messages_fts_cjk_docsize",
                "messages_fts_cjk_idx", "messages_fts_cjk_config",
            };
            for (int k = 0; k < 10; k++) json_append(objs, json_string(r[k]));
            json_set(o, "retired_cjk_objects", objs);
            out = o;
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
