#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_sessiontui_48a0e5.h"

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
        if (!strcmp(func, "wire_bytes")) {
            out = json_number((double)w9_sessiontui_48a0e5_wire_bytes(jstr(json_obj_get(in,"text"))));
        } else if (!strcmp(func, "fit_frame")) {
            const char *bs = jstr(json_obj_get(in,"budget"));
            long long budget = bs ? strtoll(bs,NULL,10) : 0;
            tmp = w9_sessiontui_48a0e5_fit_frame(jstr(json_obj_get(in,"text")), budget);
            out = tmp ? json_string(tmp) : json_null();
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
