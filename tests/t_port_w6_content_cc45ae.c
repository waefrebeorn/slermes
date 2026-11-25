/* Port gate harness for acp_adapter/content.py (gate w6_content_cc45ae). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_content_cc45ae.h"

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
        if (!strcmp(func, "resource_display_name")) {
            out = acpcont_resource_display_name(json_get_str(in, "uri", ""),
                                                json_obj_get(in, "name"), json_obj_get(in, "title"));
        } else if (!strcmp(func, "mime_main")) {
            out = acpcont_mime_main(json_obj_get(in, "mime_type"));
        } else if (!strcmp(func, "is_text_resource")) {
            out = acpcont_is_text_resource(json_obj_get(in, "mime_type"));
        } else if (!strcmp(func, "is_image_resource")) {
            out = acpcont_is_image_resource(json_obj_get(in, "mime_type"));
        } else if (!strcmp(func, "format_resource_text")) {
            out = acpcont_format_resource_text(json_get_str(in, "uri", ""),
                                               json_get_str(in, "body", ""),
                                               json_obj_get(in, "name"), json_obj_get(in, "title"),
                                               json_obj_get(in, "note"));
        } else if (!strcmp(func, "attr")) {
            out = acpcont_attr(json_obj_get(in, "obj"), json_get_str(in, "attr_name", ""));
        } else if (!strcmp(func, "image_block_to_openai_part")) {
            out = acpcont_image_block_to_openai_part(json_obj_get(in, "block"));
        } else if (!strcmp(func, "extract_text")) {
            out = acpcont_extract_text(json_obj_get(in, "prompt"));
        } else if (!strcmp(func, "content_blocks_to_openai_user_content")) {
            out = acpcont_blocks_to_openai_user_content(json_obj_get(in, "prompt"));
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
