#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_modelmetad_06ab9b.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static const char *jstr(json_t *v){ return (v && v->type==JSON_STRING) ? v->str_val : NULL; }
static int jbool(json_t *v){ return (v && v->type==JSON_BOOL) ? v->bool_val : 0; }
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func,"resolve_verify"))
            out = w11_modelmetad_06ab9b_resolve_verify(jstr(json_obj_get(in,"base_url")),
                    json_obj_get(in,"settings"), jbool(json_obj_get(in,"settings_raise")));
        else if (!strcmp(func,"total_deadline_s"))
            out = json_number(w11_modelmetad_06ab9b_total_deadline_s());
        else if (!strcmp(func,"scope_headers"))
            out = w11_modelmetad_06ab9b_scope_headers(
                    jstr(json_obj_get(in,"initial_url")), jstr(json_obj_get(in,"request_url")),
                    json_obj_get(in,"headers"));
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
