/* JSON-lines harness for the w20_provider_9c3993 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_provider_9c3993.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *str_or(json_t *in, const char *key, const char *def) {
    return json_get_str(in, key, def);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "headers")) {
            out = w20_provider_9c3993_headers(str_or(in, "api_key", ""));
        } else if (!strcmp(func, "search_request")) {
            out = w20_provider_9c3993_search_request(str_or(in, "query", ""),
                                                     json_get_num(in, "limit", 5),
                                                     json_get_bool(in, "keyless", false) ? 1 : 0,
                                                     str_or(in, "api_key", ""));
        } else if (!strcmp(func, "search_result")) {
            out = w20_provider_9c3993_search_result(json_obj_get(in, "reply"));
        } else if (!strcmp(func, "extract")) {
            out = w20_provider_9c3993_extract(json_obj_get(in, "urls"),
                                              json_obj_get(in, "replies"));
        } else if (!strcmp(func, "setup_schema")) {
            out = w20_provider_9c3993_setup_schema();
        } else if (!strcmp(func, "identity")) {
            out = w20_provider_9c3993_identity();
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
