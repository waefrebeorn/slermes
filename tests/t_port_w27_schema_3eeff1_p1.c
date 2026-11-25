/* JSON-lines harness for the w27_schema_3eeff1_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_schema_3eeff1_p1.h"

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
        if (!strcmp(func, "positive_int")) {
            out = w27_schema_3eeff1_positive_int(opt_str(in, "name"), opt_str(in, "kind"),
                                                 json_get_num(in, "num", 0.0), opt_str(in, "text"));
        } else if (!strcmp(func, "quote_ident")) {
            char *q = w27_schema_3eeff1_quote_ident(opt_str(in, "ident"));
            out = json_string(q);
            free(q);
        } else if (!strcmp(func, "legacy_inline_reinsert_sql")) {
            char *s = w27_schema_3eeff1_legacy_inline_reinsert_sql(
                opt_str(in, "table"), (int)json_get_num(in, "indent", 0.0),
                json_get_bool(in, "delete_first", false));
            out = json_string(s);
            free(s);
        } else if (!strcmp(func, "needs_narrowing")) {
            out = json_bool(w27_schema_3eeff1_fts_update_trigger_needs_narrowing(opt_str(in, "sql")));
        } else if (!strcmp(func, "complete")) {
            out = json_bool(w27_schema_3eeff1_statement_complete(opt_str(in, "sql")));
        } else if (!strcmp(func, "ddl_script")) {
            out = w27_schema_3eeff1_ddl_script_statements(opt_str(in, "ddl"));
        } else if (!strcmp(func, "ddl_settled")) {
            out = w27_schema_3eeff1_ddl_skipping_settled(opt_str(in, "ddl"), json_obj_get(in, "stored"));
        } else if (!strcmp(func, "constants")) {
            out = w27_schema_3eeff1_constants();
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
