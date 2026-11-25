/* JSON-lines harness for the w27_adapter_1f12c3_p13 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_1f12c3_p13.h"

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
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "thread_created")) {
            out = w27_adapter_1f12c3_p13_thread_created(json_obj_get(in, "thread"), json_obj_get(in, "name"));
        }
        else if (!strcmp(func, "prompt_content")) {
            out = w27_adapter_1f12c3_p13_self_contained_prompt_content(json_obj_get(in, "header"), json_obj_get(in, "body"), json_obj_get(in, "code_block"), json_obj_get(in, "tail"), json_obj_get(in, "max_len"), json_obj_get(in, "marker"));
        }
        else if (!strcmp(func, "approval_mentions")) {
            out = w27_adapter_1f12c3_p13_approval_mention_content(json_obj_get(in, "flag"), json_obj_get(in, "ids"));
        }
        else if (!strcmp(func, "forum_parts")) {
            out = w27_adapter_1f12c3_p13_forum_thread_parts(json_obj_get(in, "thread"));
        }
        else {
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
