#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_sessionhos_6fbeb0.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* A JSON string value or NULL when the key is absent / not a string, which is
 * what the live code sees as "not a str". */
static const char *str_or_null(json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

/* Python truthiness of a JSON node; a missing key is falsy. */
static int truthy(json_t *v) {
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
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;

        if (!strcmp(func, "check_claim")) {
            const char *why = w18_sessionhos_6fbeb0_check_claim(
                str_or_null(in, "actor_subject"), str_or_null(in, "room_id"));
            out = why ? json_string(why) : json_null();
        } else if (!strcmp(func, "attempt_retired")) {
            out = json_number((double)w18_sessionhos_6fbeb0_attempt_retired(
                json_obj_get(in, "task"), json_obj_get(in, "generation")));
        } else if (!strcmp(func, "retired_attempt_conflict")) {
            out = w18_sessionhos_6fbeb0_retired_attempt_conflict();
        } else if (!strcmp(func, "start_ready")) {
            out = w18_sessionhos_6fbeb0_start_ready(
                str_or_null(in, "state"), truthy(json_obj_get(in, "draining")),
                json_obj_get(in, "services"));
        } else if (!strcmp(func, "room_profiles")) {
            out = w18_sessionhos_6fbeb0_room_profiles(
                json_obj_get(in, "room"), json_obj_get(in, "local_profiles"));
        } else if (!strcmp(func, "stop_read")) {
            out = json_number((double)w18_sessionhos_6fbeb0_stop_read(
                str_or_null(in, "operation")));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }

        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func") || !strcmp(k, "out")) continue;
            fputs(",", stdout);
            emit_key(k, json_object_get_at(in, i));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
