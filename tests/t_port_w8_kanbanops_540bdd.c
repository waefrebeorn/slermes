#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_kanbanops_540bdd.h"

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
        if (!strcmp(func, "kanban_config")) {
            out = w8_kanbanops_540bdd_kanban_config(json_obj_get(in, "cfg"));
        } else if (!strcmp(func, "daemon_deprecated")) {
            out = w8_kanbanops_540bdd_daemon_deprecated(
                      json_get_bool(in, "force", false) ? 1 : 0);
        } else if (!strcmp(func, "gc_retention_error")) {
            out = w8_kanbanops_540bdd_gc_retention_error(
                      json_get_num(in, "event_days", 30), json_get_num(in, "log_days", 30));
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
