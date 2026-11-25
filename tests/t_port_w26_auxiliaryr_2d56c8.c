/* JSON-lines harness for the w26_auxiliaryr_2d56c8 gate
 * (agent/auxiliary_reasoning_floor.py).  One request object per stdin line;
 * one response line carrying the computed "out" plus every input key echoed
 * back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_auxiliaryr_2d56c8.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* Python truthiness of a JSON value. */
static int truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "route_key")) {
            out = w26_auxiliaryr_2d56c8_route_key(json_obj_get(in, "provider"), json_obj_get(in, "base_url"));
        } else if (!strcmp(func, "is_disabled")) {
            out = w26_auxiliaryr_2d56c8_is_disabled(json_obj_get(in, "config"));
        } else if (!strcmp(func, "floor_config")) {
            out = w26_auxiliaryr_2d56c8_floor_config(json_obj_get(in, "config"));
        } else if (!strcmp(func, "with_floor")) {
            out = w26_auxiliaryr_2d56c8_with_floor(json_obj_get(in, "kwargs"));
        } else if (!strcmp(func, "known_floor")) {
            out = w26_auxiliaryr_2d56c8_known_floor(json_obj_get(in, "config"), json_obj_get(in, "provider"),
                                                    json_obj_get(in, "base_url"), json_obj_get(in, "model"),
                                                    truthy(json_obj_get(in, "known")),
                                                    truthy(json_obj_get(in, "catalog")));
        } else if (!strcmp(func, "constants")) {
            out = w26_auxiliaryr_2d56c8_constants();
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
