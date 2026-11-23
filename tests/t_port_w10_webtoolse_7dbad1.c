#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_webtoolse_7dbad1.h"
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
        if (!strcmp(func, "web_extract_url")) {
            char *v = w10_webtoolse_7dbad1_web_extract_url(json_obj_get(in, "value"));
            out = v ? json_string(v) : json_null(); free(v);
        } else if (!strcmp(func, "disabled_plugin_error")) {
            char *v = w10_webtoolse_7dbad1_disabled_plugin_error(
                json_get_str(in, "capability", ""), json_get_str(in, "disabled_key", ""));
            out = json_string(v ? v : ""); free(v);
        } else if (!strcmp(func, "result_entry")) {
            out = w10_webtoolse_7dbad1_result_entry(json_get_str(in, "url", ""), json_obj_get(in, "error"));
        } else if (!strcmp(func, "extract_error_json")) {
            char *v = w10_webtoolse_7dbad1_extract_error_json(json_get_str(in, "error", ""));
            out = json_string(v ? v : ""); free(v);
        } else if (!strcmp(func, "merge_in_order")) {
            out = w10_webtoolse_7dbad1_merge_in_order((long)json_get_num(in, "total", 0),
                    json_obj_get(in, "fixed"), json_obj_get(in, "fetch_positions"),
                    json_obj_get(in, "fetch_urls"), json_obj_get(in, "results"));
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
