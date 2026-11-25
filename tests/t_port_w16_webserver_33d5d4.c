#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_webserver_33d5d4.h"

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
        if (!strcmp(func, "infer")) {
            out = w16_webserver_33d5d4_infer_type(json_obj_get(in, "value"));
        } else if (!strcmp(func, "cmdblock")) {
            out = json_bool(w16_webserver_33d5d4_is_command_provider_block(json_obj_get(in, "value")));
        } else if (!strcmp(func, "normalize")) {
            out = w16_webserver_33d5d4_normalize_config_for_web(json_obj_get(in, "config"));
        } else if (!strcmp(func, "known")) {
            out = json_bool(w16_webserver_33d5d4_endpoint_known_to_config(
                json_obj_get(in, "cfg"), json_get_str(in, "base_url", "")));
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
