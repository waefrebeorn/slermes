#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_slackcfg.h"

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
        const char *key = json_get_str(in, "key", "");
        const char *env = json_get_str(in, "env_var", "");
        json_t *out = NULL;
        char buf[4096];

        if (!strcmp(func, "flag_name")) {
            slackcfg_flag_getter_name(key, buf, sizeof(buf));
            out = json_string(buf);
        } else if (!strcmp(func, "flag_call")) {
            int strip = json_get_bool(in, "strip", false) ? 1 : 0;
            slackcfg_flag_getter_call(key, env, strip, buf, sizeof(buf));
            out = json_string(buf);
        } else if (!strcmp(func, "channel_name")) {
            slackcfg_channel_set_getter_name(key, buf, sizeof(buf));
            out = json_string(buf);
        } else if (!strcmp(func, "channel_call")) {
            int coerce = json_get_bool(in, "coerce_scalar", false) ? 1 : 0;
            slackcfg_channel_set_getter_call(key, env, coerce, buf, sizeof(buf));
            out = json_string(buf);
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
