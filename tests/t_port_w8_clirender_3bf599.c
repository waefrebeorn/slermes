#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_clirender_3bf599.h"

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
        char *s = NULL;
        if (!strcmp(func, "luminance")) {
            out = w8_clirender_3bf599_luminance_from_hex(json_get_str(in, "value", ""));
        } else if (!strcmp(func, "limit")) {
            out = w8_clirender_3bf599_coerce_output_history_limit(json_obj_get(in, "value"));
        } else if (!strcmp(func, "prepend")) {
            out = w8_clirender_3bf599_prepend_note_to_message(json_obj_get(in, "message"),
                                                            json_obj_get(in, "note"));
        } else if (!strcmp(func, "content")) {
            out = w8_clirender_3bf599_assistant_content_as_text(json_obj_get(in, "content"));
        } else if (!strcmp(func, "remap")) {
            int light = json_get_bool(in, "light", false) ? 1 : 0;
            if (!light) light = json_get_num(in, "light", 0) != 0.0 ? 1 : 0;
            s = w8_clirender_3bf599_maybe_remap_for_light_mode(json_get_str(in, "hex", ""), light);
        } else if (!strcmp(func, "detect")) {
            out = json_bool(w8_clirender_3bf599_detect_light_mode_ex(json_obj_get(in, "env"),
                    json_get_str(in, "osc_bg", "")) ? true : false);
        }
        if (s) { json_t *t = json_string(s); if (out) json_free(out); out = t; free(s); }
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
