#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_skillssync_38160e.h"

static void emit_key(const char *key, struct json_t *v) {
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
        struct json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        struct json_t *out = NULL;
        if (!strcmp(func, "safe_rel_install_path")) {
            out = w14_skillssync_38160e_safe_rel_install_path(
                json_get_str(in, "path", ""), json_get_str(in, "base", ""));
        } else if (!strcmp(func, "hub_lock_entries")) {
            out = w14_skillssync_38160e_hub_lock_entries(json_obj_get(in, "data"));
        } else if (!strcmp(func, "read_hub_install_paths")) {
            out = w14_skillssync_38160e_read_hub_install_paths(json_obj_get(in, "data"));
        } else if (!strcmp(func, "is_runtime_cache")) {
            out = json_bool(w14_skillssync_38160e_is_runtime_cache(
                      json_get_str(in, "path", ""), json_get_str(in, "skill_dir", ""),
                      json_get_bool(in, "is_dir", false) ? 1 : 0,
                      json_get_bool(in, "py_exists", false) ? 1 : 0) ? true : false);
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
