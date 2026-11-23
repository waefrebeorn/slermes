#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_statuslock_ce2ce2.h"

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
        if (!strcmp(func, "reset")) {
            w9_statuslock_ce2ce2_reset(); out = json_null();
        } else if (!strcmp(func, "reported")) {
            out = json_bool(w9_statuslock_ce2ce2_reported(jstr(json_obj_get(in,"lock_dir")))?1:0);
        } else if (!strcmp(func, "recovered")) {
            w9_statuslock_ce2ce2_recovered(jstr(json_obj_get(in,"lock_dir"))); out = json_null();
        } else if (!strcmp(func, "unusable")) {
            out = w9_statuslock_ce2ce2_unusable(jstr(json_obj_get(in,"lock_dir")),
                    (int)json_get_num(in,"errno",0), jstr(json_obj_get(in,"strerror")),
                    jstr(json_obj_get(in,"filename")));
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
