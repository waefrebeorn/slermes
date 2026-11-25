/* Port gate harness for agent/model_metadata_memo_bounds.py (gate memobounds). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_memobounds.h"

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
        if (!strcmp(func, "remember")) {
            out = memobounds_remember_endpoint_models(
                json_obj_get(in, "values"),
                json_obj_get(in, "stamps"),
                json_get_num(in, "ttl", 0.0),
                json_get_num(in, "now", 0.0),
                json_get_str(in, "memo_key", ""),
                json_obj_get(in, "cache_value"));
        } else if (!strcmp(func, "bound")) {
            out = memobounds_bound_codex_caches(
                json_obj_get(in, "context"),
                json_get_str(in, "cache_key", ""),
                (int)json_get_num(in, "limit", 128.0),
                json_obj_get(in, "max_context"));
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
