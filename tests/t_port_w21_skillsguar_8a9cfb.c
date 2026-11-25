/* JSON-lines harness for the w21_skillsguar_8a9cfb gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_skillsguar_8a9cfb.h"
#include "tools/skills_guard_credentials.c"

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
        if (!strcmp(func, "pattern_source")) {
            out = w21_skillsguar_8a9cfb_pattern_source((int)json_get_num(in, "depth", 1));
        } else if (!strcmp(func, "dotenv")) {
            out = json_bool(w21_skillsguar_8a9cfb_dotenv_match(json_get_str(in, "text", "")) != 0);
        } else if (!strcmp(func, "read_names")) {
            out = w21_skillsguar_8a9cfb_env_read_names(json_get_str(in, "line", ""));
        } else if (!strcmp(func, "raise")) {
            out = w21_skillsguar_8a9cfb_raise_reads(json_obj_get(in, "findings"),
                                                    json_obj_get(in, "lines"),
                                                    json_obj_get(in, "providers"),
                                                    json_obj_get(in, "in_text"));
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
