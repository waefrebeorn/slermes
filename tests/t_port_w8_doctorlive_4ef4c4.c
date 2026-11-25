/* t_port_w8_doctorlive_4ef4c4.c — differential harness for hermes_cli/doctor_live.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_doctorlive_4ef4c4.h"

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

        if (!strcmp(func, "classify_http_ex")) {
            const json_t *cv = json_obj_get(in, "code");
            const char *code = NULL;
            char numbuf[64];
            if (cv && cv->type == JSON_STRING) code = cv->str_val;
            else if (cv && cv->type == JSON_NUMBER) {
                snprintf(numbuf, sizeof(numbuf), "%lld", (long long)cv->num_val);
                code = numbuf;
            }
            out = w8_doctorlive_classify_http_ex(
                json_get_str(in, "name", ""), code, json_get_str(in, "key_hint", ""));
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
