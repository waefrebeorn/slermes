/* Port gate harness for agent/surface_switch.py (gate w7_surfaceswit_4b9e48). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_surfaceswit_4b9e48.h"

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
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "split_runtime_boundary")) {
            out = w7_surfaceswit_4b9e48_split_runtime_boundary(json_get_str(in, "prompt", ""));
        } else if (!strcmp(func, "runtime_host_value")) {
            char *s = w7_surfaceswit_4b9e48_runtime_host_value(
                json_get_str(in, "prompt", ""), json_get_str(in, "label", ""));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "identity_line_value")) {
            char *s = w7_surfaceswit_4b9e48_identity_line_value(
                json_get_str(in, "prompt", ""), json_get_str(in, "label", ""));
            out = json_string(s ? s : "");
            free(s);
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
