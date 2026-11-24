#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_processreg_d10a54.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static const char *sget(const json_t *o, const char *k) { return json_get_str(o, k, ""); }
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *a = json_obj_get(in, "in");
        int pexists = json_get_bool(a, "process_exists", false) ? 1 : 0;
        int stdin_open = json_get_bool(a, "stdin_open", false) ? 1 : 0;
        int avail = json_get_bool(a, "pty_available", false) ? 1 : 0;
        const char *fb = sget(a, "pty_fallback"), *ol = sget(a, "output_log");
        json_t *out = NULL;
        if (!strcmp(func, "no_pty_reason"))
            out = w16_processreg_d10a54_no_pty_reason(fb, ol, avail);
        else if (!strcmp(func, "already_exited_error"))
            out = w16_processreg_d10a54_already_exited_error(pexists, stdin_open, fb, ol, avail);
        else if (!strcmp(func, "stdin_unavailable_error"))
            out = w16_processreg_d10a54_stdin_unavailable_error(pexists, fb, ol, avail);
        else out = json_string("UNKNOWN_FUNC");
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
