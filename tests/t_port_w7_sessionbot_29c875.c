#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_sessionbot_29c875.h"

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
        const char *stem = json_get_str(in, "stem", "");
        json_t *record = json_obj_get(in, "record");
        json_t *out = NULL;
        if (!strcmp(func, "canonical_record_error")) {
            out = w7_sessionbot_29c875_canonical_record_error(stem, record,
                        json_obj_get(in, "required"));
        } else if (!strcmp(func, "record_shape_error")) {
            out = w7_sessionbot_29c875_record_shape_error(stem, record);
        } else if (!strcmp(func, "is_pre_admission_pin")) {
            out = json_bool(w7_sessionbot_29c875_is_pre_admission_pin(stem, record) != 0);
        } else if (!strcmp(func, "retry_identity")) {
            out = w7_sessionbot_29c875_retry_identity(json_get_str(in, "key", ""));
        } else if (!strcmp(func, "refused_answer")) {
            out = w7_sessionbot_29c875_refused_answer(record);
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
