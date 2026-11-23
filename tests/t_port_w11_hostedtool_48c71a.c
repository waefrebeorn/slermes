#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_hostedtool_48c71a.h"

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
        int hosted = json_get_bool(in, "hosted", false) ? 1 : 0;
        json_t *out = NULL;
        if (!strcmp(func, "message")) {
            out = w11_hostedtool_48c71a_message(json_get_str(in, "what", ""),
                                                json_get_str(in, "why", ""));
        } else if (!strcmp(func, "tool_refusal")) {
            out = w11_hostedtool_48c71a_tool_refusal(json_get_str(in, "tool_name", ""), hosted);
        } else if (!strcmp(func, "feature_enabled")) {
            out = w11_hostedtool_48c71a_feature_enabled(json_get_str(in, "feature", ""), hosted);
        } else if (!strcmp(func, "feature_refusal")) {
            out = w11_hostedtool_48c71a_feature_refusal(json_get_str(in, "feature", ""),
                                                       json_get_str(in, "what", ""), hosted);
        } else if (!strcmp(func, "require")) {
            out = w11_hostedtool_48c71a_require(json_get_str(in, "feature", ""),
                                               json_get_str(in, "what", ""), hosted);
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
