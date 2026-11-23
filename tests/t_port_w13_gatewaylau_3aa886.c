#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_gatewaylau_3aa886.h"

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
        json_t *out = NULL;
        int rc = (int)json_get_num(in, "returncode", 0);
        if (!strcmp(func, "constants")) {
            out = w13_gatewaylau_3aa886_constants();
        } else if (!strcmp(func, "label")) {
            out = w13_gatewaylau_3aa886_get_label(json_get_str(in, "suffix", ""));
        } else if (!strcmp(func, "unloaded")) {
            out = w13_gatewaylau_3aa886_error_indicates_unloaded(rc);
        } else if (!strcmp(func, "unsupported")) {
            out = w13_gatewaylau_3aa886_domain_unsupported(rc);
        } else if (!strcmp(func, "prog_args")) {
            out = w13_gatewaylau_3aa886_program_arguments(json_obj_get(in, "command"),
                        json_get_str(in, "stdout_log", ""), json_get_str(in, "stderr_log", ""));
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
