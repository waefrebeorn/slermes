/* JSON-lines harness for the w27_adapter_1f12c3_p14 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_1f12c3_p14.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "forum_thread_name")) {
            out = w27_adapter_1f12c3_p14_derive_forum_thread_name(json_obj_get(in, "message"), json_obj_get(in, "default_title"));
        }
        else if (!strcmp(func, "response_encoding")) {
            out = w27_adapter_1f12c3_p14_standalone_response_encoding(json_obj_get(in, "get_encoding"));
        }
        else if (!strcmp(func, "warn_missing_media")) {
            out = w27_adapter_1f12c3_p14_standalone_warn_missing_media(json_obj_get(in, "path"));
        }
        else if (!strcmp(func, "apply_yaml")) {
            out = w27_adapter_1f12c3_p14_apply_yaml_config(json_obj_get(in, "yaml_cfg"), json_obj_get(in, "discord_cfg"));
        }
        else {
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
