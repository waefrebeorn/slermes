#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w17_pluginsidl_dc8b64.h"

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
        if (!strcmp(func, "base_state")) {
            out = w17_pluginsidl_dc8b64_base_state(json_get_str(in, "plugin_id", ""),
                                                   json_get_str(in, "name", ""));
        } else if (!strcmp(func, "valid_name")) {
            out = json_bool(w17_pluginsidl_dc8b64_valid_name(json_get_str(in, "name", "")) ? true : false);
        } else if (!strcmp(func, "build_spec")) {
            out = w17_pluginsidl_dc8b64_build_spec(json_get_str(in, "name", ""),
                                                   json_get_str(in, "interval", ""),
                                                   json_get_str(in, "min_idle", ""),
                                                   json_get_str(in, "first_run", "defer"));
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
