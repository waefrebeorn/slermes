#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_versioninfo_941aa7.h"

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
        if (!strcmp(func, "derived_version")) {
            out = w9_versioninfo_941aa7_derived_version(
                json_get_str(in, "base", ""),
                json_get_str(in, "distance", ""),
                json_get_bool(in, "dirty", 0) ? 1 : 0,
                json_get_str(in, "short_commit", ""));
        } else if (!strcmp(func, "parse_nonnegative")) {
            out = w9_versioninfo_941aa7_parse_nonnegative(json_get_str(in, "value", ""));
        } else if (!strcmp(func, "display_version")) {
            out = w9_versioninfo_941aa7_display_version(
                json_get_str(in, "base", ""),
                json_get_str(in, "derived", ""),
                json_get_str(in, "distance", ""));
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
