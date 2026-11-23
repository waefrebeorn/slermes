#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_visionmess_0cd4b2.h"
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
        if (!strcmp(func, "is_image_part")) {
            out = json_bool(w11_visionmess_0cd4b2_is_image_part(json_obj_get(in, "part")) != 0);
        } else if (!strcmp(func, "salvage")) {
            out = w11_visionmess_0cd4b2_salvage_text_parts(
                json_obj_get(in, "content"), (int)json_get_num(in, "any_dict_text", 0));
        } else if (!strcmp(func, "content_has_image")) {
            out = json_bool(w11_visionmess_0cd4b2_content_has_image_parts(json_obj_get(in, "content")) != 0);
        } else if (!strcmp(func, "provider_model_key")) {
            out = w11_visionmess_0cd4b2_provider_model_key(
                json_get_str(in, "provider", ""), json_get_str(in, "model", ""));
        } else if (!strcmp(func, "preserve_dots")) {
            out = json_bool(w11_visionmess_0cd4b2_anthropic_preserve_dots(
                json_get_str(in, "provider", ""), json_get_str(in, "base_url", "")) != 0);
        } else if (!strcmp(func, "qwen_portal")) {
            out = json_bool(w11_visionmess_0cd4b2_is_qwen_portal(
                json_get_str(in, "base_url_lower", "")) != 0);
        }
        printf("{"); emit_key("func", json_string(func)); fputs(",", stdout); emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout); emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out); json_free(in);
    }
    return 0;
}
