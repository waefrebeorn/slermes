#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_sessiondm_83038a.h"
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
        if (!strcmp(func, "declared_topic")) {
            out = w13_sessiondm_83038a_declared_topic(json_obj_get(in, "extra"),
                json_obj_get(in, "chat_id"), json_obj_get(in, "thread_id"));
        } else if (!strcmp(func, "key_source")) {
            out = w13_sessiondm_83038a_key_source(json_obj_get(in, "source"),
                json_obj_get(in, "extra"), json_obj_get(in, "db"));
        } else if (!strcmp(func, "batch_suffix")) {
            out = w13_sessiondm_83038a_batch_suffix(json_obj_get(in, "source"),
                json_obj_get(in, "extra"));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{"); emit_key("func", json_string(func)); fputs(",", stdout); emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout); emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out); json_free(in);
    }
    return 0;
}
