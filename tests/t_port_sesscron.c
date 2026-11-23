#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_sesscron.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *elem_str(json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
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
        if (!strcmp(func, "cron_read")) {
            json_t *toks = json_obj_get(in, "tokens");
            size_t n = (toks && toks->type == JSON_ARRAY) ? json_len(toks) : 0;
            const char **arr = n ? (const char **)malloc(n * sizeof(char *)) : NULL;
            for (size_t i = 0; i < n; ++i)
                arr[i] = elem_str(json_get(toks, i));
            const char *r = sesscron_cron_read(arr, n);
            out = r ? json_string(r) : json_null();
            free(arr);
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
