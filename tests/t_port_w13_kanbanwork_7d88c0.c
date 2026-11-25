/* t_port_w13_kanbanwork_7d88c0.c — Oracle harness for hermes_cli/kanban_workflow.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_kanbanwork_7d88c0.h"
static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *wf = json_obj_get(in, "workflow");
        json_t *out = NULL;
        if (!strcmp(func, "keys")) {
            out = w13_kanbanwork_7d88c0_keys(wf);
        } else if (!strcmp(func, "column")) {
            out = w13_kanbanwork_7d88c0_column(wf, json_get_str(in, "key", ""));
        } else if (!strcmp(func, "can_move")) {
            out = w13_kanbanwork_7d88c0_can_move(wf, json_get_str(in, "src", ""),
                                                  json_get_str(in, "dst", ""));
        } else if (!strcmp(func, "to_dict")) {
            out = w13_kanbanwork_7d88c0_to_dict(wf);
        } else if (!strcmp(func, "validate")) {
            out = w13_kanbanwork_7d88c0_validate(wf);
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{"); emit_key("func", json_string(func)); fputs(",", stdout); emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout); emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out); json_free(in);
    }
    return 0;
}
