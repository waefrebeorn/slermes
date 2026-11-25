/* t_port_w18_runtimedow_f303ab.c — harness for src/gateway/runtime_downgrade_exit.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_runtimedow_f303ab.h"

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
        json_t *out = NULL;
        if (!strcmp(func, "downgrade_reason")) {
            out = w18_runtimedow_f303ab_downgrade_reason(
                json_get_bool(in, "has_skew", false), json_obj_get(in, "skew"),
                json_get_bool(in, "update_in_progress", false),
                json_get_bool(in, "predates", false));
        } else if (!strcmp(func, "tree_predates")) {
            /* read_error models an OSError from root.glob(): no sources are read,
             * which proves nothing (False), exactly like the live helper. */
            out = w18_runtimedow_f303ab_tree_predates(
                json_get_bool(in, "read_error", false) ? NULL : json_obj_get(in, "sources"));
        } else if (!strcmp(func, "watcher")) {
            int mp = (int)json_get_num(in, "max_polls", -1);
            out = w18_runtimedow_f303ab_watcher(json_get_bool(in, "running", false), mp,
                                                json_obj_get(in, "polls"));
        } else if (!strcmp(func, "constants")) {
            out = w18_runtimedow_f303ab_constants();
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
