#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_pluginsdis_6f9432.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static json_t *wrap_str(char *s) {
    json_t *out;
    if (s) { out = json_string(s); free(s); }
    else out = json_null();
    return out;
}
int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "valid_section_id")) {
            out = json_bool(w15_pluginsdis_6f9432_is_valid_system_prompt_section_id(
                                json_get_str(in, "value", "")) ? true : false);
        } else if (!strcmp(func, "format_section")) {
            out = wrap_str(w15_pluginsdis_6f9432_format_system_prompt_section(
                json_get_str(in, "section_id", ""), json_get_str(in, "content", "")));
        } else if (!strcmp(func, "format_sections")) {
            out = wrap_str(w15_pluginsdis_6f9432_format_system_prompt_sections(
                json_obj_get(in, "sections")));
        } else if (!strcmp(func, "hook_call_identity")) {
            out = wrap_str(w15_pluginsdis_6f9432_hook_call_identity(
                json_obj_get(in, "kwargs"), json_get_str(in, "hook_name", "")));
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
