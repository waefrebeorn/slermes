/*
 * t_port_w6_feishulark_1501ff.c — Oracle harness for tools/feishu_lark.py.
 * Reads JSONL fixture lines, emits {"func":...,"out":...,<echoed input keys>}.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_feishulark_1501ff.h"

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

        json_t *raw = json_obj_get(in, "raw");
        int has_raw = (raw && raw->type == JSON_OBJECT) ? 1 : 0;
        const char *content = NULL;
        if (has_raw) {
            json_t *c = json_obj_get(raw, "content");
            if (c && c->type == JSON_STRING) content = c->str_val;
        }

        if (!strcmp(func, "raw_body")) {
            out = w6_feishulark_1501ff_raw_body(content, has_raw);
        } else if (!strcmp(func, "response_data")) {
            out = w6_feishulark_1501ff_response_data(
                content, has_raw,
                json_obj_get(in, "data"),
                json_obj_get(in, "data_vars"));
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
