#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_corehealth.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
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

        if (!strcmp(func, "state_value")) {
            const char *v = corehealth_state_value(json_get_str(in, "name", ""));
            out = v ? json_string(v) : json_null();
        } else if (!strcmp(func, "state_from_value")) {
            const char *v = corehealth_state_from_value(json_get_str(in, "value", ""));
            out = v ? json_string(v) : json_null();
        } else if (!strcmp(func, "state_str")) {
            const char *v = corehealth_state_str(json_get_str(in, "name", ""));
            out = v ? json_string(v) : json_null();
        } else if (!strcmp(func, "state_count")) {
            out = json_number((double)corehealth_state_count());
        } else if (!strcmp(func, "make_probe")) {
            json_t *dj = json_obj_get(in, "detail");
            int dnull = !dj || dj->type == JSON_NULL;
            const char *detail = (dj && dj->type == JSON_STRING) ? dj->str_val : NULL;
            corehealth_probe_t p = corehealth_make_probe(
                json_get_str(in, "status", ""), detail, dnull);
            out = json_object();
            json_set(out, "status", json_string(p.status));
            json_set(out, "detail", p.detail_is_null ? json_null() : json_string(p.detail));
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
