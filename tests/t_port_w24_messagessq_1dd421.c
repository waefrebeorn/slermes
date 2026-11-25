/* JSON-lines harness for the w24_messagessq_1dd421 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_messagessq_1dd421.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "coerce_ts")) {
            out = w24_messagessq_1dd421_coerce_ts(in);
        } else if (!strcmp(func, "parse_tool_calls")) {
            out = w24_messagessq_1dd421_parse_tool_calls(json_obj_get(in, "tool_calls"));
        } else if (!strcmp(func, "tool_calls_count")) {
            out = w24_messagessq_1dd421_tool_calls_count(json_obj_get(in, "tool_calls"));
        } else if (!strcmp(func, "tool_calls_len")) {
            out = w24_messagessq_1dd421_tool_calls_len(json_obj_get(in, "raw"),
                                                       json_get_num(in, "scalar", 0.0));
        } else if (!strcmp(func, "write_columns")) {
            out = w24_messagessq_1dd421_write_columns();
        } else if (!strcmp(func, "schema_keys")) {
            out = w24_messagessq_1dd421_schema_keys();
        } else if (!strcmp(func, "constants")) {
            out = w24_messagessq_1dd421_constants();
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
