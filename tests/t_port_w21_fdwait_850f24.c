/* JSON-lines harness for the w21_fdwait_850f24 gate
 * (tools/environments/fd_wait.py).
 *
 * funcs: open {"register","errno_name","platform","is_char_device"}
 *        wait {"timeout","ready"}
 *        clamp {"timeout"}
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_fdwait_850f24.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "open")) {
            out = w21_fdwait_850f24_open_plan(in);
        } else if (!strcmp(func, "wait")) {
            out = w21_fdwait_850f24_wait_plan(in);
        } else if (!strcmp(func, "clamp")) {
            out = w21_fdwait_850f24_clamp_timeout(json_obj_get(in, "timeout"));
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
