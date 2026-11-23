#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_wguards.h"

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

        if (!strcmp(func, "reject")) {
            int outcome = (int)json_get_num(in, "outcome", 0);
            out = json_bool(wguards_write_guards_reject(outcome) ? true : false);
        } else if (!strcmp(func, "guarded_ids")) {
            json_t *ja = json_obj_get(in, "ids");
            json_t *oa = json_obj_get(in, "outcomes");
            size_t n = (ja && ja->type == JSON_ARRAY) ? json_len(ja) : 0;
            const char **ids = n ? (const char **)calloc(n, sizeof(char *)) : NULL;
            int *codes = n ? (int *)calloc(n, sizeof(int)) : NULL;
            for (size_t i = 0; i < n; ++i) {
                json_t *iv = json_get(ja, i);
                ids[i] = (iv && iv->type == JSON_STRING) ? iv->str_val : "";
                json_t *ov = (oa && oa->type == JSON_ARRAY) ? json_get(oa, i) : NULL;
                codes[i] = ov ? (int)ov->num_val : 0;
            }
            const char **res = (const char **)calloc(n + 1, sizeof(char *));
            size_t m = wguards_guarded_ids(ids, codes, n, res, n + 1);
            out = json_array();
            for (size_t i = 0; i < m; ++i) json_append(out, json_string(res[i]));
            free(res); free(ids); free(codes);
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
