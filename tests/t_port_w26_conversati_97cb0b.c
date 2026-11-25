/* JSON-lines harness for the w26_conversati_97cb0b gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back.  The line buffer is static and large
 * because one case carries a data URL over the 4 MB shrink target. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_conversati_97cb0b.h"

static char line[10 * 1024 * 1024];

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "data_url_mime")) {
            out = w26_conversati_97cb0b_data_url_mime(in);
        } else if (!strcmp(func, "source_to_data_url")) {
            out = w26_conversati_97cb0b_source_to_data_url(in);
        } else if (!strcmp(func, "write_source")) {
            out = w26_conversati_97cb0b_write_source(in);
        } else if (!strcmp(func, "decode_pixels")) {
            out = w26_conversati_97cb0b_decode_pixels(in);
        } else if (!strcmp(func, "shrink")) {
            out = w26_conversati_97cb0b_shrink(in);
        } else if (!strcmp(func, "constants")) {
            out = w26_conversati_97cb0b_constants();
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
            emit_key(k, json_object_get_at(in, i));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
