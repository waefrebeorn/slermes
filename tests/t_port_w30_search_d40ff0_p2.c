/* JSON-lines harness for the w30_search_d40ff0_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w30_search_d40ff0_p2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "filter_clauses")) {
            out = w30_search_d40ff0_p2_filter_clauses(in);
        } else if (!strcmp(func, "select_sql")) {
            out = w30_search_d40ff0_p2_select_sql(in);
        } else if (!strcmp(func, "fts_match_sql")) {
            out = w30_search_d40ff0_p2_fts_match_sql(in);
        } else if (!strcmp(func, "like_boolean")) {
            out = w30_search_d40ff0_p2_like_boolean(in);
        } else if (!strcmp(func, "like_fallback")) {
            out = w30_search_d40ff0_p2_like_fallback(in);
        } else if (!strcmp(func, "gap")) {
            out = w30_search_d40ff0_p2_gap(in);
        } else if (!strcmp(func, "cjk_like")) {
            out = w30_search_d40ff0_p2_cjk_like(in);
        } else if (!strcmp(func, "session_rank")) {
            out = w30_search_d40ff0_p2_session_rank(in);
        } else if (!strcmp(func, "order_by")) {
            out = w30_search_d40ff0_p2_order_by(in);
        } else if (!strcmp(func, "route")) {
            out = w30_search_d40ff0_p2_route(in);
        } else if (!strcmp(func, "describe_path")) {
            out = w30_search_d40ff0_p2_describe_path(in);
        } else if (!strcmp(func, "sql_constants")) {
            out = w30_search_d40ff0_p2_sql_constants();
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
