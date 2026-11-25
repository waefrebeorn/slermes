#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_runnotific_f48e0a.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "lookup_owner")) {
            out = w19_runnotific_f48e0a_lookup_owner(
                json_obj_get(in, "sessions"), json_obj_get(in, "tips"),
                json_obj_get(in, "get_raises"), json_obj_get(in, "tip_raises"),
                json_get_str(in, "session_id", ""), json_get_str(in, "current_session_id", ""),
                json_get_bool(in, "store_none", 0));
        } else if (!strcmp(func, "compression_target")) {
            out = w19_runnotific_f48e0a_compression_target(
                json_obj_get(in, "sessions"), json_obj_get(in, "tips"),
                json_obj_get(in, "entry"), json_get_str(in, "pinned_session_id", ""),
                json_get_bool(in, "get_raises", 0), json_get_bool(in, "tip_raises", 0));
        } else if (!strcmp(func, "closed_local_target")) {
            out = w19_runnotific_f48e0a_closed_local_target(
                json_obj_get(in, "sessions"), json_obj_get(in, "tips"),
                json_obj_get(in, "entry"), json_get_str(in, "pinned_session_id", ""),
                json_obj_get(in, "pinned_row"), json_get_bool(in, "tip_raises", 0));
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
