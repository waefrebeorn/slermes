#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_redhome.h"

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
        const char *path = json_get_str(in, "path", "");
        json_t *out = NULL;

        if (!strcmp(func, "is_under") || !strcmp(func, "is_under_ex")) {
            json_t *homes_node = json_obj_get(in, "homes");
            size_t n = json_is_array(homes_node) ? json_len(homes_node) : 0;
            const char **homes = n ? (const char **)malloc(n * sizeof(char *)) : NULL;
            for (size_t i = 0; i < n; i++) {
                json_t *h = json_get(homes_node, i);
                homes[i] = (h && h->type == JSON_STRING) ? h->str_val : "";
            }
            out = json_bool(redhome_is_under_hermes_home(path, homes, n) != 0);
            free(homes);
        } else if (!strcmp(func, "win_norm")) {
            char *s = redhome_win_norm(path);
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "normcase")) {
            char *s = redhome_normcase(path);
            out = json_string(s ? s : "");
            free(s);
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
