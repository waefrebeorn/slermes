/* JSON-lines harness for the w30_lostandfou_6760c6_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w30_lostandfou_6760c6_p2.h"

typedef json_t *(*laf_fn)(const json_t *);

static const struct {
    const char *name;
    laf_fn fn;
} TABLE[] = {
    {"quoted_columns", w30_lostandfou_6760c6_p2_quoted_columns},
    {"insert_sql", w30_lostandfou_6760c6_p2_insert_sql},
    {"select_sql", w30_lostandfou_6760c6_p2_select_sql},
    {"copy_columns", w30_lostandfou_6760c6_p2_copy_columns},
    {"sessions_prefix_columns", w30_lostandfou_6760c6_p2_sessions_prefix_columns},
    {"insert_prefix_plan", w30_lostandfou_6760c6_p2_insert_prefix_plan},
    {"insert_named_plan", w30_lostandfou_6760c6_p2_insert_named_plan},
    {"row_invariants_hold", w30_lostandfou_6760c6_p2_row_invariants_hold},
    {"layout_consensus", w30_lostandfou_6760c6_p2_layout_consensus},
    {"records", w30_lostandfou_6760c6_p2_records},
    {"stub_plan", w30_lostandfou_6760c6_p2_stub_plan},
    {"fts_rebuild_sql", w30_lostandfou_6760c6_p2_fts_rebuild_sql},
    {"stub_title", w30_lostandfou_6760c6_p2_stub_title},
    {"unrecognized_widths", w30_lostandfou_6760c6_p2_unrecognized_widths},
    {"recover_details", w30_lostandfou_6760c6_p2_recover_details},
    {"wal_refusal", w30_lostandfou_6760c6_p2_wal_refusal},
};

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
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
        json_free(out);
        json_free(in);
    }
    return 0;
}
