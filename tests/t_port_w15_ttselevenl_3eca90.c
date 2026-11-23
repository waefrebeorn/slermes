#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_ttselevenl_3eca90.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static long long idx(const json_t *in) { return (long long)json_get_num(in, "page_size", 20); }
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "clip")) {
            out = w15_ttselevenl_3eca90_clip(json_obj_get(in, "text"));
        } else if (!strcmp(func, "voice_row")) {
            out = w15_ttselevenl_3eca90_voice_row(json_obj_get(in, "voice"));
        } else if (!strcmp(func, "request_params")) {
            out = w15_ttselevenl_3eca90_request_params(
                      json_get_str(in, "source", ""), json_get_str(in, "query", ""),
                      json_obj_get(in, "filters"), json_get_str(in, "use_case", ""),
                      json_get_str(in, "sort", ""), json_get_str(in, "page", ""), idx(in));
        } else if (!strcmp(func, "next_page")) {
            out = w15_ttselevenl_3eca90_next_page(json_get_str(in, "source", ""),
                      json_obj_get(in, "body"), json_get_str(in, "page", ""));
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
