/*
 * t_port_w6_cachebounds_876475.c — Oracle harness for agent/cache_bounds.py.
 * Reads JSONL fixture lines, emits {"func":...,"out":...,<echoed input keys>}.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_cachebounds_876475.h"

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
        if (!strcmp(func, "drop_expired")) {
            out = w6_cachebounds_876475_drop_expired(
                json_obj_get(in, "values"), json_obj_get(in, "stamps"),
                json_get_num(in, "ttl", 0.0), json_get_num(in, "now", 0.0));
        } else if (!strcmp(func, "evict_oldest")) {
            out = w6_cachebounds_876475_evict_oldest(
                json_obj_get(in, "cache"), json_get_str(in, "key", ""),
                json_get_num(in, "limit", 0.0), json_obj_get(in, "siblings"));
        } else if (!strcmp(func, "publish_lru")) {
            out = w6_cachebounds_876475_publish_lru(
                json_obj_get(in, "table"), json_get_str(in, "key", ""),
                json_obj_get(in, "value"), json_get_num(in, "limit", 0.0),
                json_obj_get(in, "siblings"));
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
