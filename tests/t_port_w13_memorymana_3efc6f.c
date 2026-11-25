#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_memorymana_3efc6f.h"

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
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "normalize")) {
            out = w13_memorymana_3efc6f_normalize_tool_schema(json_obj_get(in, "schema"));
        } else if (!strcmp(func, "enabled")) {
            int r = w13_memorymana_3efc6f_memory_provider_tools_enabled(
                json_obj_get(in, "enabled"), json_obj_get(in, "disabled"),
                json_get_bool(in, "present", false) ? 1 : 0,
                json_obj_get(in, "resolved"));
            out = json_bool(r ? true : false);
        } else if (!strcmp(func, "tool_name")) {
            out = w13_memorymana_3efc6f_tool_name(json_obj_get(in, "tool"));
        } else if (!strcmp(func, "exposed")) {
            int r = w13_memorymana_3efc6f_memory_provider_tools_exposed(
                json_obj_get(in, "tools"), json_obj_get(in, "enabled"),
                json_obj_get(in, "disabled"), json_obj_get(in, "resolved"));
            out = json_bool(r ? true : false);
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
