/* JSON-lines harness for the w27_adapter_1f12c3_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_1f12c3_p2.h"

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
        if (!strcmp(func, "tracker_contains")) {
            out = w27_adapter_1f12c3_p2_tracker_contains(json_obj_get(in, "ids"), json_obj_get(in, "message_id"));
        }
        else if (!strcmp(func, "dave_downgraded")) {
            out = w27_adapter_1f12c3_p2_voice_receiver_dave_downgraded(json_obj_get(in, "creds"));
        }
        else if (!strcmp(func, "config_value")) {
            out = w27_adapter_1f12c3_p2_config_value(json_obj_get(in, "extra"), json_obj_get(in, "key"), json_obj_get(in, "default"), json_obj_get(in, "env"));
        }
        else if (!strcmp(func, "finite_positive_config_float")) {
            out = w27_adapter_1f12c3_p2_finite_positive_config_float(json_obj_get(in, "extra"), json_obj_get(in, "key"), json_obj_get(in, "default"), json_obj_get(in, "env"));
        }
        else if (!strcmp(func, "config_int")) {
            out = w27_adapter_1f12c3_p2_config_int(json_obj_get(in, "extra"), json_obj_get(in, "key"), json_obj_get(in, "default"), json_obj_get(in, "env"));
        }
        else if (!strcmp(func, "format_tool_preview")) {
            out = w27_adapter_1f12c3_p2_format_tool_preview(json_obj_get(in, "preview"));
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
