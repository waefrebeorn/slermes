#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_profileide_35a8d2.h"

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
        json_t *old_node = json_obj_get(in, "old");
        json_t *new_node = json_obj_get(in, "new");
        const char *old_name = (old_node && old_node->type == JSON_STRING) ? old_node->str_val : NULL;
        const char *new_name = (new_node && new_node->type == JSON_STRING) ? new_node->str_val : NULL;
        if (!strcmp(func, "moved_key")) {
            char *moved = w19_profileide_35a8d2_moved_key(json_obj_get(in, "key"), old_name, new_name);
            out = moved ? json_string(moved) : json_null();
            free(moved);
        } else if (!strcmp(func, "rekey_entry")) {
            out = w19_profileide_35a8d2_rekey_entry(json_obj_get(in, "entry"), old_name, new_name);
        } else if (!strcmp(func, "rekey_allowed")) {
            out = json_bool(w19_profileide_35a8d2_rekey_allowed(old_name, new_name) ? 1 : 0);
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
