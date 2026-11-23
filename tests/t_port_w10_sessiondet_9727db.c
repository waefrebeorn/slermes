#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "noc_w10_sessiondet_9727db.h"

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
        if (!strcmp(func, "detached_key")) {
            out = w10_sessiondet_9727db_detached_key(json_get_str(in, "route", ""), json_get_str(in, "session_id", ""));
        } else if (!strcmp(func, "detached_session_id")) {
            out = w10_sessiondet_9727db_detached_session_id(json_get_str(in, "key", ""));
        } else if (!strcmp(func, "own_detached_key")) {
            out = w10_sessiondet_9727db_own_detached_key(json_get_str(in, "key", ""), json_get_str(in, "session_id", ""));
        } else if (!strcmp(func, "detached_route_of")) {
            out = w10_sessiondet_9727db_detached_route_of(json_obj_get(in, "source"));
        } else if (!strcmp(func, "running_key")) {
            out = w10_sessiondet_9727db_running_key(json_obj_get(in, "running"), json_get_str(in, "route", ""), json_get_str(in, "session_id", ""));
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
