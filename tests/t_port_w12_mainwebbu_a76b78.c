#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_mainwebbu_a76b78.h"

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
        char *s = NULL;
        if (!strcmp(func, "project_root")) {
            s = w12_mainwebbu_a76b78_web_project_root(json_get_str(in, "web_dir", ""));
            out = json_string(s ? s : "");
        } else if (!strcmp(func, "dist_dir")) {
            s = w12_mainwebbu_a76b78_web_dist_dir(json_get_str(in, "web_dir", ""));
            out = json_string(s ? s : "");
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
        free(s);
        json_free(out);
        json_free(in);
    }
    return 0;
}
