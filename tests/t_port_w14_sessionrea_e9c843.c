#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_sessionrea_e9c843.h"

static void emit_key(const char *key, struct json_t *v) {
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
        struct json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        struct json_t *out = NULL;
        struct json_t *sess = json_obj_get(in, "session");
        struct json_t *fl = json_obj_get(in, "flags");
        if (!strcmp(func, "session_has_pending_flush")) {
            out = w14_sessionrea_e9c843_session_has_pending_flush(sess);
        } else if (!strcmp(func, "session_is_lru_evictable")) {
            out = w14_sessionrea_e9c843_session_is_lru_evictable(sess, fl);
        } else if (!strcmp(func, "session_is_reapable")) {
            out = w14_sessionrea_e9c843_session_is_reapable(sess, fl);
        } else if (!strcmp(func, "session_is_evictable")) {
            out = w14_sessionrea_e9c843_session_is_evictable(sess, json_obj_get(in, "now"),
                                          json_obj_get(in, "ttl"), fl);
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
