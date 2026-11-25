/* t_port_w16_cliinline_fa1b6a.c — harness for src/hermes_cli/cli_inline_images.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_cliinline_fa1b6a.h"

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
        if (!strcmp(func, "zero_width"))
            out = w16_cliinline_fa1b6a_zero_width(json_get_str(in, "seq", ""));
        else if (!strcmp(func, "image_rows"))
            out = w16_cliinline_fa1b6a_image_rows(json_obj_get(in, "image"));
        else if (!strcmp(func, "constants"))
            out = w16_cliinline_fa1b6a_constants();
        else
            out = json_string("UNKNOWN_FUNC");
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
        fflush(stdout);
        json_free(out); json_free(jf); json_free(in);
    }
    return 0;
}
