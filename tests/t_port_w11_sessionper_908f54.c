/* t_port_w11_sessionper_908f54.c — fixture-driven differential harness.
 * Reads one JSON case per stdin line, dispatches by "func", emits
 * {"func":..., "out":..., ...all input keys echoed...}. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_sessionper_908f54.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "override"))
            out = w11_sessionper_908f54_override(in);
        else if (!strcmp(func, "content_override"))
            out = w11_sessionper_908f54_content_override(in);
        else if (!strcmp(func, "safe_filename"))
            out = w11_sessionper_908f54_safe_filename(in);
        if (!out) out = json_null();
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func") || !strcmp(k, "out")) continue;
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
