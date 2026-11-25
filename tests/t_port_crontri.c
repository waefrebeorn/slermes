#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_crontri.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
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
        const json_t *value = json_obj_get(in, "value");
        int r = 0;
        if (!strcmp(func, "catch_up")) r = crontri_normalize_catch_up(value);
        else if (!strcmp(func, "wrap_response")) r = crontri_normalize_wrap_response(value);
        else out = json_string("UNKNOWN_FUNC");
        if (out == NULL) {
            if (r == CRONTRI_INVALID) {
                out = json_object();
                json_set(out, "ok", json_bool(false));
                json_set(out, "error", json_string(!strcmp(func, "catch_up")
                    ? "catch_up must be true, false, or null (use the default)."
                    : "wrap_response must be true, false, or null (inherit cron.wrap_response)."));
            } else {
                json_t *val = json_null();
                if (r == CRONTRI_TRUE) { json_free(val); val = json_bool(true); }
                else if (r == CRONTRI_FALSE) { json_free(val); val = json_bool(false); }
                out = json_object();
                json_set(out, "ok", json_bool(true));
                json_set(out, "value", val);
            }
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
