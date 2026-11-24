/* t_port_w16_sessionwor_8fa979.c — JSON-lines harness for gateway/session_worker_child.py port. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_sessionwor_8fa979.h"

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
        if (!strcmp(func, "limit"))
            out = json_number((double)w16_sessionwor_8fa979_limit(json_obj_get(in, "section"), json_get_str(in, "key", "")));
        else if (!strcmp(func, "root_execution_id"))
            out = w16_sessionwor_8fa979_root_execution_id(json_get_str(in, "execution_id", ""));
        else if (!strcmp(func, "is_child"))
            out = json_bool(w16_sessionwor_8fa979_is_child(json_get_str(in, "execution_id", "")) ? 1 : 0);
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
