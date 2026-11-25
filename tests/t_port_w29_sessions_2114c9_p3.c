/* JSON-lines harness for the w29_sessions_2114c9_p3 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_sessions_2114c9_p3.h"

typedef json_t *(*w29_fn)(const json_t *);

typedef struct {
    const char *name;
    w29_fn      fn;
} w29_entry;

static const w29_entry TABLE[] = {
    {"sql_literal",         w29_sessions_2114c9_p3_sql_literal},
    {"json_extract",        w29_sessions_2114c9_p3_json_extract},
    {"legacy_reset_child",  w29_sessions_2114c9_p3_legacy_reset_child},
    {"reset_child",         w29_sessions_2114c9_p3_reset_child},
    {"descendants",         w29_sessions_2114c9_p3_descendants},
    {"session_last_active", w29_sessions_2114c9_p3_session_last_active},
    {"local_chat",          w29_sessions_2114c9_p3_local_chat},
    {"owner_filter",        w29_sessions_2114c9_p3_owner_filter},
    {"roster_sqls",         w29_sessions_2114c9_p3_roster_sqls},
    {"last_active_select",  w29_sessions_2114c9_p3_last_active_select},
    {"title_refusal",       w29_sessions_2114c9_p3_title_refusal},
    {"fragments",           w29_sessions_2114c9_p3_fragments},
};

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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
