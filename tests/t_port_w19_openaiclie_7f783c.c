#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_openaiclie_7f783c.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *str_json(char *owned) {
    json_t *v = owned ? json_string(owned) : json_null();
    free(owned);
    return v;
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
        if (!strcmp(func, "realtime_url")) {
            out = json_string(w19_openaiclie_7f783c_realtime_url());
        } else if (!strcmp(func, "decode_audio")) {
            out = w19_openaiclie_7f783c_decode_audio(json_get_str(in, "b64", ""));
        } else if (!strcmp(func, "read_queue")) {
            out = w19_openaiclie_7f783c_read_queue(json_get_str(in, "text", ""),
                                                   json_get_bool(in, "exists", true),
                                                   json_get_str(in, "uuid", ""));
        } else if (!strcmp(func, "rewrite_queue")) {
            out = str_json(w19_openaiclie_7f783c_rewrite_queue(json_obj_get(in, "entries")));
        } else if (!strcmp(func, "append_processed")) {
            out = str_json(w19_openaiclie_7f783c_append_processed(json_obj_get(in, "entry"),
                                                                 json_obj_get(in, "result")));
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
