/* t_port_w19_conversati_84b9cf.c - harness for src/agent/conversation_compression_manual.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_conversati_84b9cf.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "summary_committed")) {
            out = w19_conversati_84b9cf_summary_committed(json_obj_get(in, "summary"));
        } else if (!strcmp(func, "parse")) {
            out = w19_conversati_84b9cf_parse_compress_args(json_get_str(in, "raw", ""));
        } else if (!strcmp(func, "estimate")) {
            out = w19_conversati_84b9cf_estimate_request_tokens(
                json_obj_get(in, "messages"), json_obj_get(in, "rough_map"),
                json_get_num(in, "rough_default", 0));
        } else if (!strcmp(func, "compress_now")) {
            out = w19_conversati_84b9cf_compress_now(in);
        } else if (!strcmp(func, "render")) {
            out = w19_conversati_84b9cf_render_compress_result(json_obj_get(in, "result"),
                                                              json_get_str(in, "prefix", ""));
        } else if (!strcmp(func, "props")) {
            out = w19_conversati_84b9cf_result_props(
                json_obj_get(in, "before_messages"), json_obj_get(in, "after_messages"),
                json_get_str(in, "status", ""), json_obj_get(in, "summary"));
        } else if (!strcmp(func, "constants")) {
            out = w19_conversati_84b9cf_constants();
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
        if (out) json_free(out);
        json_free(in);
    }
    return 0;
}
