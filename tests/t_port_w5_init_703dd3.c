/* t_port_w5_init_703dd3.c — differential harness for pm/__init__.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_init_703dd3.h"

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

        if (!strcmp(func, "all")) {
            size_t n = 0;
            const char *const *a = w5init_all(&n);
            json_t *arr = json_array();
            for (size_t i = 0; i < n; i++) json_append(arr, json_string(a[i]));
            out = arr;
        } else if (!strcmp(func, "home")) {
            const char *m = w5init_home(json_get_str(in, "name", ""));
            out = m ? json_string(m) : json_null();
        } else if (!strcmp(func, "has")) {
            out = json_bool(w5init_has(json_get_str(in, "name", "")) ? 1 : 0);
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
