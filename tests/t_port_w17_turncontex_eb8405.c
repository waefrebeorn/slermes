#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w17_turncontex_eb8405.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static int getbool(json_t *in, const char *k) {
    json_t *v = json_obj_get(in, k);
    return v && v->type == JSON_BOOL && v->bool_val ? 1 : 0;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func, *s;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "blocked_reason")) {
            json_t *ap = json_obj_get(in, "attempts_spent");
            int has_attempts = (ap && ap->type != JSON_NULL) ? 1 : 0;
            long long tok = 0, spent = 0;
            char *r;
            s = json_get_str(in, "tokens", "0"); if (s) tok = strtoll(s, NULL, 10);
            s = json_get_str(in, "attempts_spent", NULL); if (s) spent = strtoll(s, NULL, 10);
            r = w17_turncontex_eb8405_blocked_reason(
                getbool(in, "have_info"), getbool(in, "should"),
                json_get_str(in, "reason", NULL), getbool(in, "raise"),
                tok, has_attempts, spent);
            out = r ? json_string(r) : json_null();
            free(r);
        } else if (!strcmp(func, "codex_native")) {
            out = json_bool(w17_turncontex_eb8405_codex_native(
                json_get_str(in, "api_mode", NULL),
                json_get_str(in, "codex_auto", NULL)) ? true : false);
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
