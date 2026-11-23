#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_receipt_6c967d.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static const char *jstr(json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
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

        if (!strcmp(func, "receipt")) {
            out = w5_receipt_6c967d_receipt(jstr(in, "kind") ? jstr(in, "kind") : "",
                                            jstr(in, "update_id"),
                                            json_obj_get(in, "ops"));
        } else if (!strcmp(func, "finalized")) {
            out = w5_receipt_6c967d_finalized(jstr(in, "kind") ? jstr(in, "kind") : "",
                                              jstr(in, "update_id"),
                                              jstr(in, "outcome") ? jstr(in, "outcome") : "",
                                              (int)json_get_num(in, "exit_code", 0),
                                              json_obj_get(in, "ops"));
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
