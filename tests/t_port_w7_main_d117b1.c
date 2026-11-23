/* t_port_w7_main_d117b1.c -- JSON-lines harness for src/acp_adapter/__main__.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_main_d117b1.h"

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
        if (!strcmp(func, "doc")) {
            const char *d = w7_main_d117b1_doc();
            out = d ? json_string(d) : json_null();
        } else if (!strcmp(func, "names")) {
            out = json_array();
            size_t n = 0;
            const char *const *a = w7_main_d117b1_public_names(&n);
            for (size_t i = 0; i < n; i++) json_append(out, json_string(a[i] ? a[i] : ""));
        } else if (!strcmp(func, "has_symbol")) {
            out = json_bool(w7_main_d117b1_has_symbol(json_get_str(in, "name", "")) != 0);
        } else if (!strcmp(func, "main_calls")) {
            out = json_number((double)w7_main_d117b1_main_calls());
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
