/* JSON-lines harness for the w22_freetierta_e95c57 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_freetierta_e95c57.h"
#include "tui_gateway/free_tier_task_done.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* Python truthiness for the fixture's user_input value. */
static int truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

int main(void) {
    char line[131072];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func, *provider, *api_key, *display_kind;
        int user_input;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        provider = json_get_str(in, "provider", NULL);
        api_key = json_get_str(in, "api_key", NULL);
        display_kind = json_get_str(in, "display_kind", NULL);
        user_input = truthy(json_obj_get(in, "user_input"));
        if (!strcmp(func, "anon")) {
            out = json_bool(w22_freetierta_e95c57_is_anonymous_agent(provider, api_key) ? 1 : 0);
        } else if (!strcmp(func, "clean")) {
            out = json_bool(w22_freetierta_e95c57_is_clean_task(json_obj_get(in, "result"),
                                                               provider, api_key,
                                                               user_input, display_kind) ? 1 : 0);
        } else if (!strcmp(func, "record")) {
            out = json_bool(w22_freetierta_e95c57_should_record(json_obj_get(in, "session"),
                                                               json_obj_get(in, "result"),
                                                               provider, api_key,
                                                               display_kind) ? 1 : 0);
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
        json_free(out);
        json_free(in);
    }
    return 0;
}
