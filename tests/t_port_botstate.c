#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_botstate.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "state")) {
            json_t *v = json_obj_get(in, "value");
            json_t *ks = v ? json_obj_get(v, "kinds") : NULL;
            size_t nk = (ks && ks->type == JSON_ARRAY) ? json_len(ks) : 0;
            const char **kinds = nk ? (const char **)malloc(nk * sizeof(const char *)) : NULL;
            for (size_t i = 0; i < nk; i++) {
                json_t *e = json_get(ks, i);
                kinds[i] = (e && e->type == JSON_STRING && e->str_val) ? e->str_val : "";
            }
            out = json_string(botstate_bot_chat_state(
                json_get_bool(v, "retiring", false),
                json_get_bool(v, "draining", false),
                json_get_bool(v, "live", false),
                json_get_str(v, "exec_state", ""),
                kinds, nk));
            free(kinds);
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
