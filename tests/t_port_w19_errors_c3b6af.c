#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_errors_c3b6af.h"

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
        if (!strcmp(func, "one_line")) {
            json_t *lim = json_obj_get(in, "limit");
            long long limit = (lim && lim->type == JSON_NUMBER) ? (long long)lim->num_val : 80;
            out = w19_errors_c3b6af_one_line(json_obj_get(in, "value"), limit);
        } else if (!strcmp(func, "cause")) {
            const char *c = w19_errors_c3b6af_cause(json_get_str(in, "cls", ""));
            out = c ? json_string(c) : json_null();
        } else if (!strcmp(func, "is_kind")) {
            out = json_bool(w19_errors_c3b6af_is_kind(json_get_str(in, "cls", ""),
                                                      json_get_str(in, "base", "")) != 0);
        } else if (!strcmp(func, "in_group")) {
            out = json_bool(w19_errors_c3b6af_in_group(json_get_str(in, "cls", ""),
                                                       json_get_str(in, "group", "")) != 0);
        } else if (!strcmp(func, "message")) {
            char *m = w19_errors_c3b6af_message(json_get_str(in, "cls", ""), json_obj_get(in, "args"));
            out = m ? json_string(m) : json_null();
            free(m);
        } else if (!strcmp(func, "busy_refusal")) {
            out = w19_errors_c3b6af_busy_refusal(json_get_str(in, "exc", ""));
        } else if (!strcmp(func, "rewind_text")) {
            const char *t = w19_errors_c3b6af_rewind_text(json_get_str(in, "reason", ""));
            out = t ? json_string(t) : json_null();
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
