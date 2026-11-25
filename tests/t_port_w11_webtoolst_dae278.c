/* t_port_w11_webtoolst_dae278.c - harness for tools/web_tools_truncate.py port. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_webtoolst_dae278.h"

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
        if (!strcmp(func, "clamp")) {
            out = w11_webtoolst_dae278_clamp_char_limit(json_obj_get(in, "value"));
        } else if (!strcmp(func, "clamp_or_default")) {
            out = w11_webtoolst_dae278_clamp_or_default(json_obj_get(in, "value"));
        } else if (!strcmp(func, "effective")) {
            out = w11_webtoolst_dae278_effective_char_limit(json_obj_get(in, "char_limit"),
                     json_obj_get(in, "config_value"));
        } else if (!strcmp(func, "convert")) {
            out = w11_webtoolst_dae278_convert_base64_images(json_get_str(in, "text", ""));
        } else if (!strcmp(func, "trim")) {
            out = w11_webtoolst_dae278_trim_results(json_obj_get(in, "results"));
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
