/* JSON-lines harness for the w27_adapter_1f12c3_p9 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_1f12c3_p9.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "gate_env")) {
            out = w27_adapter_1f12c3_p9_gate_env(json_obj_get(in, "snapshot"), json_obj_get(in, "name"), json_obj_get(in, "default"), json_obj_get(in, "env"));
        }
        else if (!strcmp(func, "gate_raw")) {
            out = w27_adapter_1f12c3_p9_gate_raw(json_obj_get(in, "env_val"), json_obj_get(in, "extra"), json_obj_get(in, "extra_key"));
        }
        else if (!strcmp(func, "csv_set")) {
            out = w27_adapter_1f12c3_p9_gate_csv_set(json_obj_get(in, "raw"));
        }
        else if (!strcmp(func, "allowed_channels")) {
            out = w27_adapter_1f12c3_p9_get_allowed_channels(json_obj_get(in, "env_val"), json_obj_get(in, "extra"));
        }
        else if (!strcmp(func, "ignored_channels")) {
            out = w27_adapter_1f12c3_p9_get_ignored_channels(json_obj_get(in, "env_val"), json_obj_get(in, "extra"));
        }
        else if (!strcmp(func, "no_thread_channels")) {
            out = w27_adapter_1f12c3_p9_get_no_thread_channels(json_obj_get(in, "env_val"), json_obj_get(in, "extra"));
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
        json_free(out);
        json_free(in);
    }
    return 0;
}
