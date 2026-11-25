/* JSON-lines harness for the w26_conversati_9bd9e4 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_conversati_9bd9e4.h"

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
        if (!strcmp(func, "same_content")) {
            out = w26_conversati_9bd9e4_same_content(in);
        } else if (!strcmp(func, "tool_ids")) {
            out = w26_conversati_9bd9e4_tool_ids(json_obj_get(in, "message"));
        } else if (!strcmp(func, "result_id")) {
            out = w26_conversati_9bd9e4_result_id(json_obj_get(in, "message"));
        } else if (!strcmp(func, "reused_ids")) {
            out = w26_conversati_9bd9e4_reused_ids(json_obj_get(in, "messages"));
        } else if (!strcmp(func, "insertion_index")) {
            out = w26_conversati_9bd9e4_insertion_index(in);
        } else if (!strcmp(func, "anchor")) {
            out = w26_conversati_9bd9e4_anchor(in);
        } else if (!strcmp(func, "constants")) {
            out = w26_conversati_9bd9e4_constants();
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
