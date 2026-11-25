/* JSON-lines harness for the w16_profilepro_1706f1 gate. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_profilepro_1706f1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *str_opt(json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    if (!v || v->type != JSON_STRING) return NULL;
    return v->str_val;
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "fresh")) {
            out = json_bool(w16_profilepro_1706f1_fresh(
                str_opt(in, "clone_from"),
                json_get_bool(in, "clone_all", 0) ? 1 : 0,
                json_get_bool(in, "clone_config", 0) ? 1 : 0) ? 1 : 0);
        } else if (!strcmp(func, "refusal")) {
            out = w16_profilepro_1706f1_refusal(
                json_get_str(in, "text", ""), json_get_str(in, "type", "RuntimeError"));
        } else if (!strcmp(func, "message_from")) {
            char *s = w16_profilepro_1706f1_message_from(json_obj_get(in, "parts"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "refusal_from")) {
            out = w16_profilepro_1706f1_refusal_from(
                json_get_str(in, "type", ""), json_obj_get(in, "parts"));
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
