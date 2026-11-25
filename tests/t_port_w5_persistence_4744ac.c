#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_persistence_4744ac.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *sval(json_t *in, const char *k) {
    json_t *v = json_obj_get(in, k);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
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
        if (!strcmp(func, "store_owner_name")) {
            out = w5_persistence_4744ac_store_owner_name(sval(in, "profile_dir"),
                                                         sval(in, "default_root"));
        } else if (!strcmp(func, "state_db")) {
            out = w5_persistence_4744ac_state_db(sval(in, "key"));
        } else if (!strcmp(func, "profile_scope")) {
            out = w5_persistence_4744ac_profile_scope(sval(in, "home"),
                        sval(in, "profile"), sval(in, "default_root"), sval(in, "hermes_home"),
                        sval(in, "profile_name_for_home"), sval(in, "named_profile_home"));
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
