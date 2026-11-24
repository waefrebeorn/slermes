/* JSON-lines harness for the w21_targets_e73d84 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_targets_e73d84.h"

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
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "parse_ref")) {
            out = w21_targets_e73d84_parse_ref(json_get_str(in, "platform", ""),
                                               json_get_str(in, "target_ref", ""),
                                               json_obj_get(in, "parser"));
        } else if (!strcmp(func, "resolve")) {
            out = w21_targets_e73d84_resolve(json_get_str(in, "platform", ""),
                                             json_get_str(in, "target_ref", ""),
                                             json_get_bool(in, "pass_unresolved", false) ? 1 : 0,
                                             json_obj_get(in, "world"));
        } else if (!strcmp(func, "unknown_platform_error")) {
            out = w21_targets_e73d84_unknown_platform_error(
                json_get_str(in, "platform", ""),
                json_get_str(in, "install_hint", NULL));
        } else if (!strcmp(func, "env_probe")) {
            out = w21_targets_e73d84_env_probe();
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
            emit_key(k, json_object_get_at(in, i));
        }
        fputs("}\n", stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
