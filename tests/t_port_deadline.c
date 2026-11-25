#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_deadline.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* Decimal strings are used for 64-bit-safe numbers; accept both. */
static deadline_opt_t read_opt(json_t *in, const char *key) {
    deadline_opt_t o;
    o.present = 0;
    o.value = 0.0;
    json_t *j = json_obj_get(in, key);
    if (!j) return o;
    if (j->type == JSON_NUMBER) { o.present = 1; o.value = j->num_val; }
    else if (j->type == JSON_STRING) { o.present = 1; o.value = strtod(j->str_val, NULL); }
    return o;
}

static json_t *opt_out(deadline_opt_t o) {
    return o.present ? json_number(o.value) : json_null();
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

        if (!strcmp(func, "declared")) {
            out = opt_out(deadline_declared_floor(read_opt(in, "reg_floor")));
        } else if (!strcmp(func, "with_floor")) {
            out = opt_out(deadline_with_floor(read_opt(in, "generic"),
                                              read_opt(in, "floor")));
        } else if (!strcmp(func, "batch")) {
            deadline_opt_t generic = read_opt(in, "generic");
            json_t *ca = json_obj_get(in, "calls");
            size_t n = (ca && ca->type == JSON_ARRAY) ? json_len(ca) : 0;
            int *perr = n ? (int *)calloc(n, sizeof(int)) : NULL;
            deadline_opt_t *decl = n ? (deadline_opt_t *)calloc(n, sizeof(deadline_opt_t)) : NULL;
            for (size_t i = 0; i < n; ++i) {
                json_t *c = json_get(ca, i);
                perr[i] = json_get_bool(c, "parse_error", false) ? 1 : 0;
                decl[i] = deadline_declared_floor(read_opt(c, "reg_floor"));
            }
            out = opt_out(deadline_batch(generic, perr, decl, n));
            free(perr); free(decl);
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
