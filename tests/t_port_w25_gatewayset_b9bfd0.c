/* JSON-lines harness for the w25_gatewayset_b9bfd0 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_gatewayset_b9bfd0.h"

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
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "is_setting")) {
            out = w25_gatewayset_b9bfd0_is_setting(json_obj_get(in, "canonical"));
        } else if (!strcmp(func, "split_arg")) {
            out = w25_gatewayset_b9bfd0_split_arg(json_obj_get(in, "cmd"));
        } else if (!strcmp(func, "mirror_busy")) {
            out = w25_gatewayset_b9bfd0_mirror_busy(json_obj_get(in, "arg"));
        } else if (!strcmp(func, "unbounded")) {
            out = w25_gatewayset_b9bfd0_unbounded(json_obj_get(in, "canonical"),
                                                  json_obj_get(in, "arg"),
                                                  json_obj_get(in, "cron_read"));
        } else if (!strcmp(func, "call_command")) {
            out = w25_gatewayset_b9bfd0_call_command(json_obj_get(in, "canonical"),
                                                     json_obj_get(in, "arg"));
        } else if (!strcmp(func, "render")) {
            out = w25_gatewayset_b9bfd0_render(json_obj_get(in, "canonical"),
                                               json_obj_get(in, "reply"),
                                               json_obj_get(in, "stopped"));
        } else if (!strcmp(func, "constants")) {
            out = w25_gatewayset_b9bfd0_constants();
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
