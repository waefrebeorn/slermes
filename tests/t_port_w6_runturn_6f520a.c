#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_runturn_6f520a.h"

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
        if (!strcmp(func, "overflow")) {
            int v = w6_runturn_is_context_overflow(
                json_obj_get(in, "result"),
                (long long)json_get_num(in, "history_len", 0));
            out = json_bool(v != 0);
        } else if (!strcmp(func, "bound")) {
            out = w6_runturn_bound_model_input(
                json_obj_get(in, "history"),
                (long long)json_get_num(in, "limit", 0));
        } else if (!strcmp(func, "reason")) {
            char *s = w6_runturn_hygiene_no_commit_reason(json_obj_get(in, "attrs"));
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
