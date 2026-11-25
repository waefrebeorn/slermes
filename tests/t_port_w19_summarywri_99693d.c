#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_summarywri_99693d.h"

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
        if (!strcmp(func, "parse_bool")) {
            json_t *value = json_obj_get(in, "value");
            int def = json_get_bool(in, "default", 0) ? 1 : 0;
            out = json_bool(w19_summarywri_99693d_parse_bool(value, def));
        } else if (!strcmp(func, "text")) {
            char *s = w19_summarywri_99693d_text(json_obj_get(in, "value"),
                                                json_get_str(in, "fallback", "(no summary)"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "bullet_lines")) {
            out = w19_summarywri_99693d_bullet_lines(json_obj_get(in, "values"));
        } else if (!strcmp(func, "title")) {
            char *s = w19_summarywri_99693d_title(json_obj_get(in, "payload"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "render_markdown")) {
            char *s = w19_summarywri_99693d_render_markdown(json_obj_get(in, "payload"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "render_html")) {
            char *s = w19_summarywri_99693d_render_html(json_obj_get(in, "payload"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "resolve_config")) {
            out = w19_summarywri_99693d_resolve_delivery_config(json_obj_get(in, "platform_cfg"),
                                                               json_obj_get(in, "config"),
                                                               json_obj_get(in, "env"));
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
