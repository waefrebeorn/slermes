#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_runtime_3ded45.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *str_json(char *owned) {
    json_t *v = owned ? json_string(owned) : json_null();
    free(owned);
    return v;
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
        if (!strcmp(func, "derive")) {
            const char *kind = json_get_str(in, "kind", "local");
            int k = !strcmp(kind, "api") ? W19_RUNTIME_3DED45_API
                  : !strcmp(kind, "room") ? W19_RUNTIME_3DED45_ROOM
                  : W19_RUNTIME_3DED45_LOCAL;
            out = str_json(w19_runtime_3ded45_derive(k, json_get_str(in, "session_id", ""),
                                                     json_obj_get(in, "identity"),
                                                     json_get_str(in, "profile_id", ""),
                                                     json_get_str(in, "principal_id", ""),
                                                     json_get_str(in, "base_request_id", ""),
                                                     (long long)json_get_num(in, "generation", 0)));
        } else if (!strcmp(func, "mutation_call")) {
            out = w19_runtime_3ded45_mutation_call(json_get_str(in, "operation", NULL),
                                                   json_obj_get(in, "payload"),
                                                   json_get_bool(in, "payload_callable", false));
        } else if (!strcmp(func, "legacy_prepared")) {
            out = w19_runtime_3ded45_legacy_prepared(json_get_bool(in, "is_none", false),
                                                     json_obj_get(in, "receipt"),
                                                     json_get_str(in, "target", NULL),
                                                     (long long)json_get_num(in, "target_revision", 0),
                                                     json_obj_get(in, "values"));
        } else if (!strcmp(func, "chat_params")) {
            out = w19_runtime_3ded45_chat_params(json_get_str(in, "request_id", NULL),
                                                 json_get_str(in, "operation", NULL),
                                                 json_obj_get(in, "payload"),
                                                 (long long)json_get_num(in, "expected_revision", 0),
                                                 (long long)json_get_num(in, "expected_generation", 0));
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
