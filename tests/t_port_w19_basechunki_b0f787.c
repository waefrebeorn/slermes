/* t_port_w19_basechunki_b0f787.c - harness for src/gateway/platforms/base_chunking.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_basechunki_b0f787.h"

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
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        int utf16 = json_get_bool(in, "utf16", false);
        json_t *out = NULL;
        if (!strcmp(func, "chunk")) {
            out = w19_basechunki_b0f787_chunk_markdown_message(
                json_get_str(in, "content", ""), (long long)json_get_num(in, "max_length", 4096),
                utf16);
        } else if (!strcmp(func, "open_link")) {
            out = w19_basechunki_b0f787_open_link_bracket(json_get_str(in, "text", ""));
        } else if (!strcmp(func, "fence_state")) {
            out = w19_basechunki_b0f787_fence_state_after(
                json_get_str(in, "text", ""), json_get_bool(in, "in_code", false),
                json_get_str(in, "lang", ""));
        } else if (!strcmp(func, "row_start")) {
            out = w19_basechunki_b0f787_table_row_start(
                json_get_str(in, "text", ""), (long long)json_get_num(in, "split_at", 0));
        } else if (!strcmp(func, "rollback")) {
            out = w19_basechunki_b0f787_table_rollback_point(
                json_get_str(in, "text", ""), (long long)json_get_num(in, "split_at", 0),
                (long long)json_get_num(in, "max_units", 0), utf16);
        } else if (!strcmp(func, "header_repeat")) {
            out = w19_basechunki_b0f787_table_header_to_repeat(
                json_get_str(in, "chunk", ""), json_get_str(in, "rest", ""),
                (long long)json_get_num(in, "max_units", 0), utf16);
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
        if (out) json_free(out);
        json_free(in);
    }
    return 0;
}
