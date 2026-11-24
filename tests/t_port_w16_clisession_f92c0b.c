#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_clisession_f92c0b.h"

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
        if (!strcmp(func, "squash")) {
            out = w16_clisession_f92c0b_squash(json_get_str(in, "text", ""),
                    (int)json_get_num(in, "limit", 120.0));
        } else if (!strcmp(func, "undo")) {
            out = w16_clisession_f92c0b_undo_content_to_text(json_obj_get(in, "content"));
        } else if (!strcmp(func, "title")) {
            out = w16_clisession_f92c0b_title_session_id(
                    json_get_str(in, "session_id", ""),
                    json_obj_get(in, "tip"),
                    json_get_bool(in, "db", 1) ? 1 : 0);
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
