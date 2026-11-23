#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_sessionlist_96be7b.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static const char *jstr(json_t *v){ return (v && v->type==JSON_STRING) ? v->str_val : NULL; }
static int jbool(json_t *v){ return (v && v->type==JSON_BOOL && v->bool_val) ? 1 : 0; }

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        char *tmp = NULL;
        if (!strcmp(func, "check_page_params")) {
            out = w9_sessionlist_96be7b_check_page_params(json_obj_get(in,"params"));
        } else if (!strcmp(func, "binding_digest")) {
            tmp = w9_sessionlist_96be7b_binding_digest(jstr(json_obj_get(in,"workspace")),
                     jstr(json_obj_get(in,"query")), jbool(json_obj_get(in,"include_hidden")));
            out = tmp ? json_string(tmp) : json_null();
        } else if (!strcmp(func, "encode_cursor")) {
            const char *os = jstr(json_obj_get(in,"offset"));
            long long off = os ? strtoll(os,NULL,10) : 0;
            tmp = w9_sessionlist_96be7b_encode_cursor(off,
                     jstr(json_obj_get(in,"workspace")), jstr(json_obj_get(in,"query")),
                     jbool(json_obj_get(in,"include_hidden")));
            out = tmp ? json_string(tmp) : json_null();
        } else if (!strcmp(func, "decode_cursor")) {
            out = w9_sessionlist_96be7b_decode_cursor(jstr(json_obj_get(in,"cursor")),
                     jstr(json_obj_get(in,"workspace")), jstr(json_obj_get(in,"query")),
                     jbool(json_obj_get(in,"include_hidden")));
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
        free(tmp);
        json_free(out);
        json_free(in);
    }
    return 0;
}
