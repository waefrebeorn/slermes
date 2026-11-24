/* t_port_w17_browserses_dec4eb.c — harness for src/tools/browser_session_sandbox.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w17_browserses_dec4eb.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        int rc = (int)json_get_num(in, "rc", 0);
        const char *text = json_get_str(in, "text", "");
        json_t *out = NULL;
        if (!strcmp(func, "shared_key"))
            out = w17_browserses_dec4eb_shared_key(json_get_str(in, "env_id", ""),
                        json_get_str(in, "udd", ""));
        else if (!strcmp(func, "read_devtools"))
            out = w17_browserses_dec4eb_read_devtools(rc, text);
        else if (!strcmp(func, "live_devtools"))
            out = w17_browserses_dec4eb_live_devtools(rc, text, json_get_bool(in, "raise_exec", false));
        else if (!strcmp(func, "constants"))
            out = w17_browserses_dec4eb_constants();
        else
            out = json_string("UNKNOWN_FUNC");
        printf("{");
        json_t *jf = json_string(func);
        emit_key("func", jf);
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out); json_free(jf); json_free(in);
    }
    return 0;
}
