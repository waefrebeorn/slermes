#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_admissions_c58422.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* A string the caller may leave out or send as null: NULL then, never "". */
static const char *opt_str(json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    if (!v || v->type == JSON_NULL) return NULL;
    return v->type == JSON_STRING ? v->str_val : NULL;
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
        if (!strcmp(func, "raise_gate")) {
            int kind = (int)json_get_num(in, "gate_kind", 0);
            out = w19_admissions_c58422_raise_gate(kind, opt_str(in, "reason"),
                                                  opt_str(in, "field"), opt_str(in, "detail"));
        } else if (!strcmp(func, "refusal")) {
            out = w19_admissions_c58422_refusal(opt_str(in, "reason"), opt_str(in, "field"),
                                                opt_str(in, "detail"));
        } else if (!strcmp(func, "steer_replayed")) {
            out = json_bool(w19_admissions_c58422_steer_replayed(opt_str(in, "status"),
                                                                 opt_str(in, "outcome")));
        } else if (!strcmp(func, "coerce_record")) {
            out = w19_admissions_c58422_coerce_record(opt_str(in, "intent"), opt_str(in, "status"));
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
