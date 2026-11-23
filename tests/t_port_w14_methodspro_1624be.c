#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_methodspro_1624be.h"

static void emit_key(const char *key, struct json_t *v) {
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
        struct json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        struct json_t *out = NULL;
        if (!strcmp(func, "message_row_id")) {
            out = w14_methodspro_1624be_message_row_id(json_obj_get(in, "msg"));
        } else if (!strcmp(func, "row_ids_of")) {
            out = w14_methodspro_1624be_row_ids_of(json_obj_get(in, "messages"));
        } else if (!strcmp(func, "pdf_page_range")) {
            out = w14_methodspro_1624be_pdf_page_range(json_obj_get(in, "params"), json_obj_get(in, "max_pages"));
        } else if (!strcmp(func, "preview_restart_prompt")) {
            out = w14_methodspro_1624be_preview_restart_prompt(json_obj_get(in, "url"), json_obj_get(in, "cwd"),
                json_obj_get(in, "context"), json_obj_get(in, "has_history"),
                json_obj_get(in, "note"), json_obj_get(in, "rules"));
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
