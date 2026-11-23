#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_auxiliaryc_f6bead.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *obj_str(const json_t *in, const char *container, const char *key) {
    const json_t *c = json_obj_get(in, container);
    if (c && c->type == JSON_OBJECT && json_has(c, key)) return json_get_str(c, key, "");
    return NULL;
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
        if (!strcmp(func, "embedded")) {
            int objmode = json_has(in, "error_object");
            out = w15_auxiliaryc_f6bead_embedded_provider_error(
                json_obj_get(in, "response"), objmode,
                obj_str(in, "error_object", "message"),
                obj_str(in, "error_object", "code"));
        } else if (!strcmp(func, "is_cancellation")) {
            out = json_bool(w15_auxiliaryc_f6bead_is_cancellation(
                json_get_str(in, "kind", "")) ? 1 : 0);
        } else if (!strcmp(func, "error_class")) {
            out = w15_auxiliaryc_f6bead_error_class(
                json_get_str(in, "kind", ""), json_get_str(in, "stub_reason", "unknown"));
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
