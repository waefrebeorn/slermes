#include "hermes_json.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_hermesconst_d195ca.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *sval(json_t *in, const char *k, const char *dflt) {
    json_t *v = json_obj_get(in, k);
    return (v && v->type == JSON_STRING) ? v->str_val : dflt;
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
        if (!strcmp(func, "under")) {
            out = w5_hermesconst_d195ca_under(sval(in, "path", ""), sval(in, "root", ""),
                                               sval(in, "sep", "/"));
        } else if (!strcmp(func, "same_birth")) {
            out = w5_hermesconst_d195ca_same_birth(json_get_num(in, "now", 0.0),
                        json_get_num(in, "started", 0.0),
                        json_get_bool(in, "has_proc", false) ? 1 : 0);
        } else if (!strcmp(func, "scan_subtree")) {
            out = w5_hermesconst_d195ca_scan_subtree(in);
        } else if (!strcmp(func, "subtree_touch_state")) {
            out = w5_hermesconst_d195ca_subtree_touch_state(in);
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
