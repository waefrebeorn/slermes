#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_sessionhis_503dfd.h"

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
        if (!strcmp(func, "image_url")) {
            out = w15_sessionhis_503dfd_history_part_image_url(json_obj_get(in, "part"));
        } else if (!strcmp(func, "dict_text")) {
            out = w15_sessionhis_503dfd_history_dict_text(json_obj_get(in, "content"),
                                                           json_get_bool(in, "image_urls", 1));
        } else if (!strcmp(func, "display_text")) {
            out = w15_sessionhis_503dfd_content_display_text(json_obj_get(in, "content"));
        } else if (!strcmp(func, "coerce")) {
            out = w15_sessionhis_503dfd_coerce_message_text(json_obj_get(in, "content"),
                                                             json_get_bool(in, "image_urls", 1));
        } else if (!strcmp(func, "hidden")) {
            out = w15_sessionhis_503dfd_is_display_hidden_marker(
                      json_get_str(in, "role", ""), json_get_str(in, "text", ""));
        } else if (!strcmp(func, "strip_screenshots")) {
            out = w15_sessionhis_503dfd_strip_trailing_screenshot_placeholders(
                      json_get_str(in, "text", ""));
        } else if (!strcmp(func, "seed_history")) {
            out = w15_sessionhis_503dfd_coerce_seed_history(json_obj_get(in, "value"));
        } else if (!strcmp(func, "inflight")) {
            out = w15_sessionhis_503dfd_inflight_text(json_obj_get(in, "value"));
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
