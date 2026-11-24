#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_doctorconn_8245d8.h"

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
        if (!strcmp(func, "row")) {
            out = w18_doctorconn_8245d8_row(json_get_str(in, "name", ""),
                                            json_get_str(in, "status", "ok"),
                                            json_get_str(in, "detail", ""),
                                            json_obj_get(in, "label"));
        } else if (!strcmp(func, "openrouter_status")) {
            out = w18_doctorconn_8245d8_openrouter_status((int)json_get_num(in, "status_code", 0));
        } else if (!strcmp(func, "probe_openrouter")) {
            out = w18_doctorconn_8245d8_probe_openrouter(
                json_get_str(in, "key", ""), json_get_str(in, "error", ""),
                (int)json_get_num(in, "status_code", 0));
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
