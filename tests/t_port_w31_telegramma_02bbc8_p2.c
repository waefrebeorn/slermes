/* JSON-lines harness for the w31_telegramma_02bbc8_p2 gate: the pure text
 * primitives of telegram_markdown (escaping, URL tail, link and quote-edge
 * predicates). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w31_telegramma_02bbc8_p2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "escape_mdv2")) {
            char *s = w31_telegramma_02bbc8_p2_escape_mdv2(json_get_str(in, "text", ""));
            out = json_string(s);
            free(s);
        } else if (!strcmp(func, "url_end")) {
            out = json_number((double)w31_telegramma_02bbc8_p2_url_end(json_get_str(in, "url", "")));
        } else if (!strcmp(func, "link_target_renders")) {
            out = json_bool(w31_telegramma_02bbc8_p2_link_target_renders(json_get_str(in, "target", "")));
        } else if (!strcmp(func, "crosses_quote_edge")) {
            long a = (long)json_get_num(in, "start", 0);
            long b = (long)json_get_num(in, "end", 0);
            out = json_bool(w31_telegramma_02bbc8_p2_crosses_quote_edge(json_get_str(in, "text", ""), a, b));
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
