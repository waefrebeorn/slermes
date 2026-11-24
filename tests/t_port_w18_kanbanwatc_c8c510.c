/* t_port_w18_kanbanwatc_c8c510.c — harness for src/gateway/kanban_watchers_common.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_kanbanwatc_c8c510.h"

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
        json_t *boards = json_obj_get(in, "boards");
        json_t *metadata = json_obj_get(in, "metadata");
        json_t *cfg = json_obj_get(in, "cfg");
        if (!strcmp(func, "list_boards")) {
            out = w18_kanbanwatc_c8c510_list_boards(json_get_bool(in, "list_raises", false), boards, metadata);
        } else if (!strcmp(func, "board_slugs")) {
            out = w18_kanbanwatc_c8c510_board_slugs(json_get_bool(in, "list_raises", false),
                    boards, metadata, json_get_str(in, "default_board", ""));
        } else if (!strcmp(func, "positive_int")) {
            out = w18_kanbanwatc_c8c510_positive_int(cfg, json_get_str(in, "key", ""));
        } else if (!strcmp(func, "auto_decompose")) {
            out = w18_kanbanwatc_c8c510_auto_decompose(json_get_bool(in, "config_error", false), cfg);
        } else if (!strcmp(func, "gc_retention")) {
            out = w18_kanbanwatc_c8c510_gc_retention(json_get_bool(in, "config_error", false), cfg);
        } else if (!strcmp(func, "dispatch_allowed")) {
            out = w18_kanbanwatc_c8c510_dispatch_allowed(json_get_bool(in, "import_error", false),
                                                         json_get_bool(in, "paused", false));
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
