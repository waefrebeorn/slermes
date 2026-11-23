#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_geminilong_ef98e5.h"

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
        if (!strcmp(func, "max_output_tokens")) {
            long long v = w10_geminilong_ef98e5_long_decode_max_output_tokens(
                json_get_str(in, "model", ""), json_obj_get(in, "config"));
            out = (v < 0) ? json_null() : json_number((double)v);
        } else if (!strcmp(func, "hops")) {
            out = json_number((double)w10_geminilong_ef98e5_long_decode_hops(json_obj_get(in, "value")));
        } else if (!strcmp(func, "stale_multiplier")) {
            out = json_number((double)w10_geminilong_ef98e5_long_decode_stale_multiplier(json_obj_get(in, "payload")));
        } else if (!strcmp(func, "continuation_token")) {
            char *s = w10_geminilong_ef98e5_continuation_token(json_obj_get(in, "payload"));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "merge_hop_usage")) {
            out = w10_geminilong_ef98e5_merge_hop_usage(json_obj_get(in, "hops"));
        } else if (!strcmp(func, "merge_hop_responses")) {
            out = w10_geminilong_ef98e5_merge_hop_responses(json_obj_get(in, "hops"));
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
