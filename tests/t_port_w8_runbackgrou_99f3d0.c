/* Port gate harness for gateway/run_background.py (gate w8_runbackgrou_99f3d0). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_runbackgrou_99f3d0.h"

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
        if (!strcmp(func, "bg_preview")) {
            out = runbackgrou_bg_prompt_preview(json_get_str(in, "prompt", NULL),
                     (int)json_get_num(in, "limit", 60));
        } else if (!strcmp(func, "complete_preview")) {
            out = runbackgrou_complete_preview(json_get_str(in, "prompt", NULL));
        } else if (!strcmp(func, "image_paths")) {
            out = runbackgrou_media_image_paths(json_obj_get(in, "urls"),
                                                json_obj_get(in, "media_types"));
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
