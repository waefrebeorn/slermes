#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w17_jobsonesho_343221.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[1 << 20];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func, *s;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "wedged")) {
            char *text = w17_jobsonesho_343221_write_wedged(
                json_obj_get(in, "job"), json_get_str(in, "now_iso", ""));
            out = text ? json_string(text) : json_null();
            free(text);
        } else if (!strcmp(func, "missed")) {
            long long grace = 0;
            char *text;
            s = json_get_str(in, "grace", "0"); if (s) grace = strtoll(s, NULL, 10);
            text = w17_jobsonesho_343221_write_missed(
                json_obj_get(in, "job"), json_get_str(in, "next_run", ""),
                json_get_str(in, "now_iso", ""), grace);
            out = text ? json_string(text) : json_null();
            free(text);
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
