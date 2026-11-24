/* t_port_w19_schemahist_ddcbcb.c - harness for src/persistence/sqlite/admin/_schema_history.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_schemahist_ddcbcb.h"

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
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        const char *table = json_get_str(in, "table", "");
        json_t *out = NULL;
        if (!strcmp(func, "apply")) {
            out = w19_schemahist_ddcbcb_apply(json_obj_get(in, "columns"), json_obj_get(in, "edits"));
        } else if (!strcmp(func, "snapshots")) {
            out = w19_schemahist_ddcbcb_declared_snapshots(table);
        } else if (!strcmp(func, "current")) {
            out = w19_schemahist_ddcbcb_current_declared_columns(table);
        } else if (!strcmp(func, "reachable")) {
            json_t *lim = json_obj_get(in, "limit");
            double limit = (lim && lim->type == JSON_NUMBER) ? lim->num_val : 1000.0;
            out = w19_schemahist_ddcbcb_reachable_physical_layouts(
                table, json_obj_get(in, "accept"), limit);
        } else if (!strcmp(func, "history")) {
            out = w19_schemahist_ddcbcb_schema_history(table);
        } else if (!strcmp(func, "tables")) {
            out = w19_schemahist_ddcbcb_tables();
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
        if (out) json_free(out);
        json_free(in);
    }
    return 0;
}
