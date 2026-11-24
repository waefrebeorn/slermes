#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_profilerep_3b4c1c.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static int jbool(json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k);
    return v ? json_get_bool(o, k, false) : 0;
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
        if (!strcmp(func, "stored_profile")) {
            out = w19_profilerep_3b4c1c_stored_profile(json_obj_get(in, "value"));
        } else if (!strcmp(func, "find_crossed")) {
            out = w19_profilerep_3b4c1c_find_crossed(json_obj_get(in, "rows"),
                                                     json_get_str(in, "owner", ""),
                                                     json_obj_get(in, "ledgered"));
        } else if (!strcmp(func, "less_topic_rows")) {
            out = w19_profilerep_3b4c1c_less_topic_rows(json_obj_get(in, "rows"),
                                                        jbool(in, "has_profile"));
        } else if (!strcmp(func, "key_profiles")) {
            out = w19_profilerep_3b4c1c_key_profiles(json_obj_get(in, "session_keys"));
        } else if (!strcmp(func, "import_moved")) {
            out = w19_profilerep_3b4c1c_import_moved(json_obj_get(in, "session"),
                                                     json_get_str(in, "profile_name", ""),
                                                     jbool(in, "id_present"),
                                                     jbool(in, "parent_present"),
                                                     jbool(in, "title_present"));
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
