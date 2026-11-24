/* JSON-lines harness for the w29_sessions_2114c9_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_sessions_2114c9_p1.h"

typedef json_t *(*w29_fn)(const json_t *);

typedef struct {
    const char *name;
    w29_fn      fn;
} w29_entry;

static const w29_entry TABLE[] = {
    {"stripped",           w29_sessions_2114c9_p1_stripped},
    {"json_value",         w29_sessions_2114c9_p1_json_value},
    {"tool_pin",           w29_sessions_2114c9_p1_tool_pin},
    {"ids",                w29_sessions_2114c9_p1_ids},
    {"removed",            w29_sessions_2114c9_p1_removed},
    {"listed",             w29_sessions_2114c9_p1_listed},
    {"optional_list",      w29_sessions_2114c9_p1_optional_list},
    {"sql_limit",          w29_sessions_2114c9_p1_sql_limit},
    {"last_active",        w29_sessions_2114c9_p1_last_active},
    {"parse_model_config", w29_sessions_2114c9_p1_parse_model_config},
};

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
        json_t *in, *out = NULL;
        const char *func;
        size_t i;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        for (i = 0; i < sizeof TABLE / sizeof TABLE[0]; i++) {
            if (!strcmp(func, TABLE[i].name)) { out = TABLE[i].fn(in); break; }
        }
        if (!out) out = json_string("UNKNOWN_FUNC");
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (i = 0; i < json_object_size(in); i++) {
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
