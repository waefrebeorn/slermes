/* tests/t_port_w33_maintenanc_3e1b75_p1.c
 * JSON-lines harness for persistence/sqlite/housekeeping/maintenance.py
 * builders (gate w33_maintenanc_3e1b75_p1).  One request per stdin line; one
 * response line carrying "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w33_maintenanc_3e1b75_p1.h"

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
        if (!strcmp(func, "escape_like")) {
            out = w33_maintenanc_3e1b75_p1_escape_like(json_get_str(in, "text", ""));
        } else if (!strcmp(func, "like")) {
            out = w33_maintenanc_3e1b75_p1_like(json_get_str(in, "value", ""));
        } else if (!strcmp(func, "placeholders")) {
            out = w33_maintenanc_3e1b75_p1_placeholders(json_get_num(in, "count", 0));
        } else if (!strcmp(func, "id_chunks")) {
            out = w33_maintenanc_3e1b75_p1_id_chunks(json_obj_get(in, "ids"),
                                                     json_get_num(in, "size", 900));
        } else if (!strcmp(func, "cwd_prefix_clause")) {
            out = w33_maintenanc_3e1b75_p1_cwd_prefix_clause(json_get_str(in, "prefix", ""));
        } else if (!strcmp(func, "last_active_sql")) {
            out = w33_maintenanc_3e1b75_p1_last_active_sql(json_get_str(in, "alias", "s"));
        } else if (!strcmp(func, "continuation_edge")) {
            out = w33_maintenanc_3e1b75_p1_continuation_edge();
        } else if (!strcmp(func, "continued_ancestors_sql")) {
            out = w33_maintenanc_3e1b75_p1_continued_ancestors_sql(json_get_str(in, "where", ""));
        } else if (!strcmp(func, "pinned_tail_sql")) {
            out = w33_maintenanc_3e1b75_p1_pinned_tail_sql();
        } else if (!strcmp(func, "not_pinned_sql")) {
            out = w33_maintenanc_3e1b75_p1_not_pinned_sql(json_get_str(in, "alias", "s"));
        } else if (!strcmp(func, "row_bytes_sql")) {
            out = w33_maintenanc_3e1b75_p1_row_bytes_sql(json_obj_get(in, "columns"));
        } else if (!strcmp(func, "projection_fields")) {
            out = w33_maintenanc_3e1b75_p1_projection_fields(json_obj_get(in, "available"));
        } else if (!strcmp(func, "sweep_scope")) {
            out = w33_maintenanc_3e1b75_p1_sweep_scope(in);
        } else if (!strcmp(func, "idle_archive_sql")) {
            out = w33_maintenanc_3e1b75_p1_idle_archive_sql(in);
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
