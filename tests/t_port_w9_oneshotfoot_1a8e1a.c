#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_oneshotfoot_1a8e1a.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *opt_str(json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
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
        if (!strcmp(func, "is_single_query")) {
            out = json_bool(w9_oneshotfoot_1a8e1a_is_single_query_ex(
                        opt_str(in, "policy_source"), opt_str(in, "env_value")));
        } else if (!strcmp(func, "agent_is_oneshot")) {
            int cached = -1;
            if (json_get_bool(in, "cached_present", false))
                cached = json_get_bool(in, "cached", false) ? 1 : 0;
            out = json_bool(w9_oneshotfoot_1a8e1a_agent_is_oneshot(
                        cached, json_get_bool(in, "computed", false)));
        } else if (!strcmp(func, "disabled_toolsets")) {
            out = w9_oneshotfoot_1a8e1a_disabled_toolsets();
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
