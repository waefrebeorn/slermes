#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_redactconns_217bf3.h"

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
        int cf = json_get_bool(in, "code_file", false) ? 1 : 0;
        if (!strcmp(func, "closes")) {
            out = w7_redactconns_217bf3_closes_wrapper(json_get_str(in, "value", ""), (int)json_get_num(in, "j", 0));
        } else if (!strcmp(func, "value_end")) {
            out = w7_redactconns_217bf3_kv_value_end(json_get_str(in, "value", ""), (int)json_get_num(in, "i", 0),
                  json_get_bool(in, "semicolon", false) ? 1 : 0);
        } else if (!strcmp(func, "line")) {
            out = w7_redactconns_217bf3_redact_line(json_get_str(in, "value", ""), cf);
        } else if (!strcmp(func, "strings")) {
            out = w7_redactconns_217bf3_redact_strings(json_get_str(in, "value", ""), cf);
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
