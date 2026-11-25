/* JSON-lines harness for the w21_approvalwi_56c32a gate.
 * One request object per stdin line; one response per line carrying "out" plus
 * every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_approvalwi_56c32a.h"

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
        if (!strcmp(func, "hardline_patterns")) {
            out = w21_approvalwi_56c32a_windows_hardline_patterns(json_get_str(in, "cmdpos", ""));
        } else if (!strcmp(func, "strict_patterns")) {
            out = w21_approvalwi_56c32a_windows_whatif_strict_patterns(json_get_str(in, "cmdpos", ""));
        } else if (!strcmp(func, "win_cmdpos")) {
            char *s = w21_approvalwi_56c32a_win_cmdpos(json_get_str(in, "cmdpos", ""));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "whatif")) {
            out = w21_approvalwi_56c32a_whatif_preview_is_real(json_get_str(in, "command", ""));
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
