/* JSON-lines harness for the w21_kanbandbde_905b79 gate.
 * One request object per stdin line; one response per line carrying "out" plus
 * every input key echoed back. Missing keys read as None and never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_kanbandbde_905b79.h"
#include "hermes_cli/kanban_db_default_assignee.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "apply")) {
            out = w21_kanbandbde_905b79_apply(json_get_str(in, "task_id", ""),
                                              json_get_str(in, "assignee", ""),
                                              json_get_bool(in, "dry_run", 0) ? 1 : 0,
                                              (int)json_get_num(in, "rowcount", -1),
                                              json_get_bool(in, "raises", 0) ? 1 : 0);
        } else if (!strcmp(func, "fill")) {
            out = w21_kanbandbde_905b79_fill(json_get_str(in, "task_id", ""),
                                             json_obj_get(in, "default_assignee"),
                                             json_get_bool(in, "dry_run", 0) ? 1 : 0,
                                             (int)json_get_num(in, "rowcount", -1),
                                             json_get_bool(in, "raises", 0) ? 1 : 0,
                                             json_obj_get(in, "current_row"),
                                             json_obj_get(in, "result"));
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
