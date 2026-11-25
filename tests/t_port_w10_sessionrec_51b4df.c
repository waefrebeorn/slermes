#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_sessionrec_51b4df.h"

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
        if (!strcmp(func, "profile_from_key"))
            out = w10_sessionrec_51b4df_profile_from_session_key(json_get_str(in, "session_key", NULL));
        else if (!strcmp(func, "recovered_allowed"))
            out = w10_sessionrec_51b4df_recovered_row_allowed(
                json_get_str(in, "requested_session_key", NULL),
                json_obj_get(in, "recovered"),
                json_get_bool(in, "multiplex_profiles", false) ? 1 : 0,
                json_get_str(in, "active_profile", NULL));
        else if (!strcmp(func, "matches_scope"))
            out = w10_sessionrec_51b4df_recovered_row_matches_source_scope(
                json_obj_get(in, "recovered"), json_obj_get(in, "source"));
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
