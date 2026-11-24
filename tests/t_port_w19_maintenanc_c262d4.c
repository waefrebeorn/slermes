/* tests/t_port_w19_maintenanc_c262d4.c - JSON-lines harness for persistence/sqlite/admin/_maintenance.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_maintenanc_c262d4.h"

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
        if (!strcmp(func, "next_chunk_rows")) {
            out = w19_maintenanc_c262d4_next_chunk_rows(
                json_get_num(in, "rows", 0), json_get_num(in, "cost_s", 0),
                json_get_num(in, "target_s", 0));
        } else if (!strcmp(func, "teardown_info")) {
            out = w19_maintenanc_c262d4_teardown_info(
                json_get_str(in, "table", ""), (long long)json_get_num(in, "table_index", 0),
                (long long)json_get_num(in, "table_count", 0),
                (long long)json_get_num(in, "total", 0),
                (long long)json_get_num(in, "drained", 0));
        } else if (!strcmp(func, "vacuum_info")) {
            out = w19_maintenanc_c262d4_vacuum_info(
                json_get_num(in, "size_bytes", 0), json_get_num(in, "estimated_bytes", 0),
                json_get_num(in, "elapsed", 0));
        } else if (!strcmp(func, "logical_size")) {
            out = w19_maintenanc_c262d4_logical_size(
                json_get_num(in, "page_count", 0), json_get_num(in, "page_size", 0),
                json_get_bool(in, "ok", 1) ? 1 : 0);
        } else if (!strcmp(func, "freelist_ratio")) {
            out = w19_maintenanc_c262d4_freelist_ratio(
                json_get_num(in, "page_count", 0), json_get_num(in, "freelist_count", 0),
                json_get_bool(in, "ok", 1) ? 1 : 0);
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
