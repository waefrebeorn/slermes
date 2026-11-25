#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_webappliste_3df92f.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static const char *jstr(json_t *v){ return (v && v->type==JSON_STRING) ? v->str_val : NULL; }
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        char *tmp = NULL;
        if (!strcmp(func, "bare_host")) {
            tmp = w9_webappliste_3df92f_bare_host(jstr(json_obj_get(in,"host")));
            out = tmp ? json_string(tmp) : json_null();
        } else if (!strcmp(func, "is_wildcard")) {
            out = json_bool(w9_webappliste_3df92f_is_wildcard(jstr(json_obj_get(in,"host")))?1:0);
        } else if (!strcmp(func, "is_loopback")) {
            out = json_bool(w9_webappliste_3df92f_is_loopback(jstr(json_obj_get(in,"host")))?1:0);
        } else if (!strcmp(func, "dial_host")) {
            tmp = w9_webappliste_3df92f_dial_host(jstr(json_obj_get(in,"host")));
            out = tmp ? json_string(tmp) : json_null();
        } else if (!strcmp(func, "dial_origin")) {
            tmp = w9_webappliste_3df92f_dial_origin(jstr(json_obj_get(in,"host")),
                     jstr(json_obj_get(in,"port")));
            out = tmp ? json_string(tmp) : json_null();
        } else if (!strcmp(func, "public_url")) {
            out = w9_webappliste_3df92f_public_url(json_obj_get(in,"value"));
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
        free(tmp);
        json_free(out);
        json_free(in);
    }
    return 0;
}
