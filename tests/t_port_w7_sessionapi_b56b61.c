#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_sessionapi_b56b61.h"

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
        if (!strcmp(func, "api_generation_id")) {
            out = w7_sessionapi_b56b61_api_generation_id(
                json_get_str(in, "session_id", ""), (int)json_get_num(in, "generation", 0));
        } else if (!strcmp(func, "room_session_id")) {
            out = w7_sessionapi_b56b61_room_session_id(
                json_obj_get(in, "identity"), (int)json_get_num(in, "generation", 0));
        } else if (!strcmp(func, "api_storage_source")) {
            out = w7_sessionapi_b56b61_api_storage_source(
                json_obj_get(in, "store"), json_get_str(in, "session_id", ""),
                json_get_str(in, "fallback", ""));
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
