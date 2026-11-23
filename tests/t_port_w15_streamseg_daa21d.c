#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_streamseg_daa21d.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static long long get_i64(const json_t *in, const char *key) {
    return (long long)strtoll(json_get_str(in, key, "0"), NULL, 10);
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
        if (!strcmp(func, "utf16_len")) {
            out = json_number((double)w15_streamseg_daa21d_utf16_len(
                json_get_str(in, "text", "")));
        } else if (!strcmp(func, "start")) {
            out = w15_streamseg_daa21d_start(json_get_str(in, "turn_id", ""));
        } else if (!strcmp(func, "delta")) {
            out = w15_streamseg_daa21d_delta(json_get_str(in, "turn_id", ""),
                get_i64(in, "segment"), get_i64(in, "offset"));
        } else if (!strcmp(func, "delta_advance")) {
            const json_t *t = json_obj_get(in, "text");
            int isstr = t && t->type == JSON_STRING;
            out = json_number((double)w15_streamseg_daa21d_delta_advance(
                get_i64(in, "offset"), json_get_str(in, "text", ""), isstr));
        } else if (!strcmp(func, "seal")) {
            out = w15_streamseg_daa21d_seal(json_get_str(in, "turn_id", ""),
                get_i64(in, "segment"));
        } else if (!strcmp(func, "stamp_reply")) {
            const json_t *p = json_obj_get(in, "payload");
            out = w15_streamseg_daa21d_stamp_reply(json_get_bool(in, "has_segments", 0),
                json_get_str(in, "turn_id", ""), get_i64(in, "segment"), get_i64(in, "offset"),
                json_get_str(in, "event", ""), p);
        } else if (!strcmp(func, "stamp_session")) {
            out = w15_streamseg_daa21d_stamp_session(json_obj_get(in, "session"),
                json_get_str(in, "event", ""), json_obj_get(in, "payload"));
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
