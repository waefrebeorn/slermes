/* t_port_w11_methodsi18_f41712.c — harness for the methods_i18n C port. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_methodsi18_f41712.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *sval(json_t *o, const char *k) {
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
        const char *rid = json_get_str(in, "rid", "");
        const char *fail = sval(in, "fail");
        json_t *out = NULL;
        if (!strcmp(func, "languages")) {
            out = w11_methodsi18_f41712_languages(rid, json_obj_get(in, "languages"), fail);
        } else if (!strcmp(func, "catalog")) {
            out = w11_methodsi18_f41712_catalog(
                rid, json_obj_get(in, "params"), json_obj_get(in, "surfaces"),
                sval(in, "core"), sval(in, "resolve"), json_obj_get(in, "messages"), fail);
        } else if (!strcmp(func, "constants")) {
            out = json_string(w11_methodsi18_f41712_i18n_err());
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
