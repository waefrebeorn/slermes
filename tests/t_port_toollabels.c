#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_toollabels.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static json_t *cstr(char *s) { json_t *o = json_string(s ? s : ""); free(s); return o; }

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "app_title")) {
            out = cstr(toollabels_app_title(json_get_str(in, "slug", "")));
        } else if (!strcmp(func, "action_phrase")) {
            out = cstr(toollabels_action_phrase(json_get_str(in, "tool", ""),
                                                json_get_str(in, "app_slug", "")));
        } else if (!strcmp(func, "label")) {
            out = toollabels_label_for_tool_name(json_get_str(in, "name", ""),
                        json_obj_get(in, "arguments"),
                        json_get_str(in, "verb", ""), json_get_str(in, "emoji", ""),
                        json_get_str(in, "preview", ""));
        } else if (!strcmp(func, "bridge")) {
            out = toollabels_bridge_labels(json_get_str(in, "function_name", ""),
                                           json_obj_get(in, "args"));
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
