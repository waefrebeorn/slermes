#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_codeexecut_132ad4.h"

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
        if (!strcmp(func, "is_secret")) {
            out = json_bool(w11_codeexecut_132ad4_is_secret(json_get_str(in, "name", "")) ? true : false);
        } else if (!strcmp(func, "cache_probe")) {
            out = w11_codeexecut_132ad4_cache_probe(json_obj_get(in, "cache"),
                        json_get_str(in, "key", ""), json_copy(json_obj_get(in, "value")));
        } else if (!strcmp(func, "scrub")) {
            out = w11_codeexecut_132ad4_scrub(json_obj_get(in, "source_env"),
                    json_obj_get(in, "passthrough"), json_obj_get(in, "resolved"),
                    json_obj_get(in, "scoped_additions"), json_obj_get(in, "toolchain"),
                    json_get_bool(in, "is_windows", false) ? 1 : 0,
                    json_get_bool(in, "delegated", false) ? 1 : 0,
                    json_get_str(in, "os_task", ""), json_get_str(in, "fence_root", "1"));
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
