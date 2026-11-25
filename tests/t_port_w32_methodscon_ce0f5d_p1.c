/* JSON-lines harness for the w32_methodscon_ce0f5d_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w32_methodscon_ce0f5d_p1.h"

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
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "normalize_word")) {
            out = w32_methodscon_ce0f5d_p1_normalize_word(json_obj_get(in, "value"));
        } else if (!strcmp(func, "normalize_raw_word")) {
            out = w32_methodscon_ce0f5d_p1_normalize_raw_word(json_obj_get(in, "value"));
        } else if (!strcmp(func, "bool_word")) {
            out = w32_methodscon_ce0f5d_p1_bool_word(json_obj_get(in, "value"));
        } else if (!strcmp(func, "service_tier_word")) {
            out = w32_methodscon_ce0f5d_p1_service_tier_word(json_obj_get(in, "tier"));
        } else if (!strcmp(func, "fast_set")) {
            out = w32_methodscon_ce0f5d_p1_fast_set(json_obj_get(in, "rid"), json_obj_get(in, "value"),
                                                    json_obj_get(in, "current_tier"));
        } else if (!strcmp(func, "verbose_set")) {
            out = w32_methodscon_ce0f5d_p1_verbose_set(json_obj_get(in, "rid"), json_obj_get(in, "value"),
                                                       json_obj_get(in, "current"));
        } else if (!strcmp(func, "word_set")) {
            out = w32_methodscon_ce0f5d_p1_word_set(json_obj_get(in, "rid"), json_obj_get(in, "key"),
                                                    json_obj_get(in, "value"));
        } else if (!strcmp(func, "details_section")) {
            out = w32_methodscon_ce0f5d_p1_details_section(json_obj_get(in, "rid"), json_obj_get(in, "key"),
                                                           json_obj_get(in, "value"));
        } else if (!strcmp(func, "resolve_key")) {
            out = w32_methodscon_ce0f5d_p1_resolve_key(json_obj_get(in, "key"));
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
