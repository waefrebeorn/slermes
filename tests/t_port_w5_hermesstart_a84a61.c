#include "hermes_json.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_hermesstart_a84a61.h"

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

/* NULL for a missing or JSON-null field (needed to distinguish timeout_s=None). */
static const char *sval_null(json_t *in, const char *k) {
    json_t *v = json_obj_get(in, k);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
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
        if (!strcmp(func, "disabled")) {
            out = w5_hermesstart_a84a61_disabled(sval(in, "env_value"));
        } else if (!strcmp(func, "resolve_timeout")) {
            out = w5_hermesstart_a84a61_resolve_timeout(sval(in, "raw"));
        } else if (!strcmp(func, "dump_path")) {
            out = w5_hermesstart_a84a61_dump_path(sval(in, "home"));
        } else if (!strcmp(func, "kick_extra")) {
            out = w5_hermesstart_a84a61_kick_extra(sval(in, "extra_s"));
        } else if (!strcmp(func, "lease_clamp")) {
            out = w5_hermesstart_a84a61_lease_clamp(sval(in, "expected_s"));
        } else if (!strcmp(func, "arm_timeout")) {
            out = w5_hermesstart_a84a61_arm_timeout(sval_null(in, "timeout_s"),
                        json_get_bool(in, "disabled", false) ? 1 : 0, sval(in, "env_raw"));
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
