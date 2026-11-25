/* JSON-lines harness for the w30_lostandfou_6760c6_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w30_lostandfou_6760c6_p1.h"

typedef json_t *(*laf_fn)(const json_t *);

static const struct {
    const char *name;
    laf_fn fn;
} TABLE[] = {
    {"is_session_id", w30_lostandfou_6760c6_p1_is_session_id},
    {"looks_like_source", w30_lostandfou_6760c6_p1_looks_like_source},
    {"session_source_index", w30_lostandfou_6760c6_p1_session_source_index},
    {"classify_row", w30_lostandfou_6760c6_p1_classify_row},
    {"heuristic_started_at", w30_lostandfou_6760c6_p1_heuristic_started_at},
    {"parse_sql_default", w30_lostandfou_6760c6_p1_parse_sql_default},
    {"notnull_substitutes", w30_lostandfou_6760c6_p1_notnull_substitutes},
    {"declared_types", w30_lostandfou_6760c6_p1_declared_types},
    {"cell_fits", w30_lostandfou_6760c6_p1_cell_fits},
    {"type_conflicts", w30_lostandfou_6760c6_p1_type_conflicts},
    {"sentinel_holds", w30_lostandfou_6760c6_p1_sentinel_holds},
    {"text_shape_holds", w30_lostandfou_6760c6_p1_text_shape_holds},
    {"predicate", w30_lostandfou_6760c6_p1_predicate},
    {"version_vulnerable", w30_lostandfou_6760c6_p1_version_vulnerable},
};

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
        json_t *in, *out = NULL;
        const char *func;
        size_t i;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        for (i = 0; i < sizeof(TABLE) / sizeof(TABLE[0]); i++) {
            if (!strcmp(func, TABLE[i].name)) {
                out = TABLE[i].fn(in);
                break;
            }
        }
        if (!out) out = json_string("UNKNOWN_FUNC");
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (i = 0; i < json_object_size(in); i++) {
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
