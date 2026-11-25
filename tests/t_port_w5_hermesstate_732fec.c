#include "hermes_json.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_hermesstate_732fec.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *sval(json_t *in, const char *k) {
    json_t *v = json_obj_get(in, k);
    return (v && v->type == JSON_STRING) ? v->str_val : "";
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
        if (!strcmp(func, "release")) {
            out = w5_hermesstate_732fec_release(json_get_bool(in, "is_none", false) ? 1 : 0);
        } else if (!strcmp(func, "acquire_variant")) {
            out = w5_hermesstate_732fec_acquire_variant(
                        json_get_bool(in, "first_warning_present", false) ? 1 : 0,
                        json_get_bool(in, "first_warning_result", false) ? 1 : 0,
                        sval(in, "removal_release"), sval(in, "migration_guide"));
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
