#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_freetierb_22d217.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static int jbool(const json_t *in, const char *key) {
    const json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_BOOL && v->bool_val) ? 1 : 0;
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
        if (!strcmp(func, "free_tier_route")) {
            out = json_bool(w18_freetierb_22d217_free_tier_route(
                jbool(in, "free_tier_account"), json_get_str(in, "inference_provider", "")) ? 1 : 0);
        } else if (!strcmp(func, "as_payload")) {
            out = w18_freetierb_22d217_as_payload(json_obj_get(in, "record"));
        } else if (!strcmp(func, "build_record")) {
            out = w18_freetierb_22d217_build_record(
                jbool(in, "other"), json_obj_get(in, "state"), json_obj_get(in, "failure"),
                jbool(in, "free_active"), jbool(in, "is_guest"),
                json_get_str(in, "inference_provider", ""), json_get_num(in, "finished_at", 0.0));
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
