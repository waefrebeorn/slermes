#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_mcptooltr_2238d8.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static const char *jstr(json_t *v){ return (v && v->type==JSON_STRING) ? v->str_val : NULL; }
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
        if (!strcmp(func, "content_type_base")) {
            tmp = w11_mcptooltr_2238d8_content_type_base(jstr(json_obj_get(in, "content_type")));
            out = tmp ? json_string(tmp) : json_string("");
        } else if (!strcmp(func, "is_2xx")) {
            json_t *st = json_obj_get(in, "status");
            int code = (st && st->type == JSON_NUMBER) ? (int)st->num_val : 0;
            out = json_bool(w11_mcptooltr_2238d8_is_2xx(code));
        } else if (!strcmp(func, "present")) {
            out = w11_mcptooltr_2238d8_present(json_obj_get(in, "kwargs"));
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
