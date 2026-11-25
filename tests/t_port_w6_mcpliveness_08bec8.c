/* Port gate harness for tools/mcp_liveness.py (gate w6_mcpliveness_08bec8). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_mcpliveness_08bec8.h"

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
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "action")) {
            out = mcpliveness_action(json_get_str(in, "state", ""),
                                     json_get_str(in, "app_name", ""),
                                     json_get_str(in, "gpu_label", ""));
        } else if (!strcmp(func, "availability_problem")) {
            out = mcpliveness_availability_problem(json_get_str(in, "state", ""));
        } else if (!strcmp(func, "parse")) {
            out = mcpliveness_parse(json_obj_get(in, "facts"));
        } else if (!strcmp(func, "describe")) {
            out = mcpliveness_describe(json_obj_get(in, "facts"));
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
