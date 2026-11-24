#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w17_backendret_ac8bfc.h"

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
        int preparing = json_get_bool(in, "preparing", false) ? 1 : 0;
        int has_token = json_get_bool(in, "has_token", false) ? 1 : 0;
        int active = (int)json_get_num(in, "active", 0);
        int committed = json_get_bool(in, "committed", false) ? 1 : 0;
        double now = json_get_num(in, "now", 0);
        double expires = json_get_num(in, "expires", 0);
        json_t *tn = json_obj_get(in, "token");
        int token_null = (!tn || tn->type == JSON_NULL) ? 1 : 0;
        const char *token = (tn && tn->type == JSON_STRING) ? tn->str_val : "";
        const char *cur = json_get_str(in, "cur", "");
        if (!strcmp(func, "acquire")) {
            out = w17_backendret_ac8bfc_acquire(preparing, has_token, active, committed, now, expires);
        } else if (!strcmp(func, "release")) {
            out = w17_backendret_ac8bfc_release(active);
        } else if (!strcmp(func, "commit")) {
            out = w17_backendret_ac8bfc_commit(token, token_null, has_token, cur, committed, now, expires);
        } else if (!strcmp(func, "cancel")) {
            out = w17_backendret_ac8bfc_cancel(token, token_null, has_token, cur, committed, now, expires);
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
