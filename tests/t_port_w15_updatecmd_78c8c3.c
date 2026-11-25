#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_updatecmd_78c8c3.h"

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
        if (!strcmp(func, "tracking_refspec")) {
            out = w15_updatecmd_78c8c3_tracking_refspec(
                json_get_str(in, "remote", ""), json_get_str(in, "branch", ""));
        } else if (!strcmp(func, "is_shallow_repository")) {
            out = w15_updatecmd_78c8c3_is_shallow_repository(json_get_str(in, "git_stdout", ""));
        } else if (!strcmp(func, "compare_ref_exists")) {
            out = w15_updatecmd_78c8c3_compare_ref_exists(
                (int)strtoll(json_get_str(in, "returncode", "0"), NULL, 10));
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
