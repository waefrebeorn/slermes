/* JSON-lines harness for the w21_telegram_15532d gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back.
 *
 * The port source is included by bare name (the repo's harness convention) so
 * the gate links against libjson alone. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_telegram_15532d.h"
#include "persistence/sqlite/conversation/_store/telegram.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "normalize")) {
            char *s = w21_telegram_15532d_normalize_profile(json_obj_get(in, "value"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "params")) {
            out = w21_telegram_15532d_params(json_obj_get(in, "user_id"),
                                             json_obj_get(in, "profile"),
                                             json_obj_get(in, "limit"));
        } else if (!strcmp(func, "query")) {
            out = w21_telegram_15532d_unlinked_query(
                json_get_bool(in, "profile_scoped", true),
                json_get_str(in, "preview", ""),
                json_get_str(in, "last_active", ""));
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
