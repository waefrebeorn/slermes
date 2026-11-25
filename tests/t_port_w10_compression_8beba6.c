#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_compression_8beba6.h"

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
        if (!strcmp(func, "elision_marker")) {
            char *s = w10_compression_8beba6_elision_marker(
                strtoll(json_get_str(in, "omitted", "0"), NULL, 10),
                strtoll(json_get_str(in, "total", "0"), NULL, 10));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "elision_marker_max_len")) {
            out = json_number((double)w10_compression_8beba6_elision_marker_max_len());
        } else if (!strcmp(func, "elide")) {
            char *s = w10_compression_8beba6_elide(
                json_get_str(in, "text", ""),
                strtoll(json_get_str(in, "limit", "0"), NULL, 10));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "elide_middle")) {
            char *s = w10_compression_8beba6_elide_middle(
                json_get_str(in, "text", ""),
                strtoll(json_get_str(in, "head", "0"), NULL, 10),
                strtoll(json_get_str(in, "tail", "0"), NULL, 10));
            out = json_string(s ? s : "");
            free(s);
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
