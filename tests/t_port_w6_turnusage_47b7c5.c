/*
 * t_port_w6_turnusage_47b7c5.c -- JSON-lines oracle harness for the C11 port
 * of agent/turn_usage.py. Int64 token counts arrive as decimal strings.
 */
#include "noc_w6_turnusage_47b7c5.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static long long get_i64(const json_t *in, const char *key) {
    const char *s = json_get_str(in, key, "0");
    return strtoll(s ? s : "0", NULL, 10);
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

        if (!strcmp(func, "add")) {
            out = w6_turnusage_47b7c5_add_session_usage(
                get_i64(in,"session_prompt_tokens"), get_i64(in,"session_completion_tokens"),
                get_i64(in,"session_total_tokens"), get_i64(in,"session_input_tokens"),
                get_i64(in,"session_output_tokens"), get_i64(in,"session_cache_read_tokens"),
                get_i64(in,"session_cache_write_tokens"), get_i64(in,"session_reasoning_tokens"),
                get_i64(in,"prompt_tokens"), get_i64(in,"completion_tokens"),
                get_i64(in,"total_tokens"), get_i64(in,"input_tokens"),
                get_i64(in,"output_tokens"), get_i64(in,"cache_read_tokens"),
                get_i64(in,"cache_write_tokens"), get_i64(in,"reasoning_tokens"));
        } else if (!strcmp(func, "write_tier")) {
            json_t *ttl = json_obj_get(in, "ttl");
            char *text = ttl ? json_serialize(ttl) : strdup("[]");
            const char *tier = json_get_str(in, "tier", "");
            out = w6_turnusage_47b7c5_with_write_tier(text, get_i64(in, "cache_write_tokens"),
                                                       tier);
            free(text);
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
