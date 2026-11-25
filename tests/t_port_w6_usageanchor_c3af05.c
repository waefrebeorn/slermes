/* Port gate harness for agent/usage_anchor.py (gate w6_usageanchor_c3af05). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_usageanchor_c3af05.h"

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
        if (!strcmp(func, "fp")) {
            out = usageanchor_fingerprint(json_obj_get(in, "msg"));
        } else if (!strcmp(func, "capture")) {
            out = usageanchor_capture(json_obj_get(in, "prompt_tokens"),
                                      json_obj_get(in, "completion_tokens"),
                                      json_obj_get(in, "messages"));
        } else if (!strcmp(func, "match")) {
            out = usageanchor_anchor_matches(json_obj_get(in, "messages"),
                                             json_obj_get(in, "anchor"));
        } else if (!strcmp(func, "ser")) {
            out = usageanchor_serialize(json_obj_get(in, "anchor"));
        } else if (!strcmp(func, "ctx")) {
            out = usageanchor_context_tokens(json_obj_get(in, "messages"),
                                             json_obj_get(in, "anchor"),
                                             json_get_num(in, "delta_estimate", 0));
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
