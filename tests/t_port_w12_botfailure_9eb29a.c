#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_botfailure_9eb29a.h"

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
        json_t *out = NULL;
        char *s = NULL;
        if (!strcmp(func, "retry_action")) {
            s = w12_botfailure_9eb29a_retry_action(json_get_str(in, "reason", ""));
        } else if (!strcmp(func, "turn_failure_text")) {
            s = w12_botfailure_9eb29a_turn_failure_text(
                    json_get_str(in, "stdout", ""), json_get_str(in, "stderr", ""));
        } else if (!strcmp(func, "classify")) {
            s = w12_botfailure_9eb29a_classify_agent_error(json_get_str(in, "text", ""));
        } else if (!strcmp(func, "result_retry")) {
            s = w12_botfailure_9eb29a_result_retry_action(json_obj_get(in, "result"));
        }
        out = json_string(s ? s : "");
        free(s);
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
