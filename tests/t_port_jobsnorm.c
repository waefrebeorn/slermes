#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_jobsnorm.h"

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
        if (!strcmp(func, "opt")) {
            /* json_get_str returns NULL for a non-string, matching Python's
             * isinstance(value, str) guard. */
            char *s = jobsnorm_normalize_optional_text(
                json_get_str(in, "value", NULL),
                json_get_bool(in, "strip", false));
            if (s) { out = json_string(s); free(s); }
        } else if (!strcmp(func, "base")) {
            char *s = jobsnorm_normalize_base_url(json_get_str(in, "value", NULL));
            if (s) { out = json_string(s); free(s); }
        } else if (!strcmp(func, "str_list")) {
            out = jobsnorm_normalize_str_list(json_obj_get(in, "items"));
        } else if (!strcmp(func, "context")) {
            out = jobsnorm_normalize_context_from(json_obj_get(in, "value"));
        } else if (!strcmp(func, "failure")) {
            char *s = jobsnorm_normalize_failure_deliver(json_obj_get(in, "value"));
            if (s) { out = json_string(s); free(s); }
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
