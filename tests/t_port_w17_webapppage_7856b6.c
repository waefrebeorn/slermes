/* t_port_w17_webapppage_7856b6.c — harness for src/hermes_cli/webapp_page_policy.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w17_webapppage_7856b6.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
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
        if (!strcmp(func, "script_hashes")) {
            out = w17_webapppage_7856b6_script_hashes(json_get_str(in, "html", ""));
        } else if (!strcmp(func, "page_headers")) {
            json_t *us = json_obj_get(in, "ui_surface");
            const char *surface = (us && us->type == JSON_STRING && us->str_val) ? us->str_val : NULL;
            out = w17_webapppage_7856b6_page_headers_ex(surface,
                        json_get_str(in, "html", ""), json_obj_get(in, "headers"));
        } else if (!strcmp(func, "constants")) {
            out = w17_webapppage_7856b6_constants();
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        json_t *jf = json_string(func);
        emit_key("func", jf);
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out); json_free(jf); json_free(in);
    }
    return 0;
}
