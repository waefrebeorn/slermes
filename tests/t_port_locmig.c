/* t_port_locmig.c — locmig gate harness (local_migration.py). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_locmig.h"

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
        int refused = 0;
        if (!strcmp(func, "legacy_lineage_root")) {
            int has_legacy = json_obj_get(in, "legacy_session_id") != NULL;
            const char *legacy = json_get_str(in, "legacy_session_id", NULL);

            /* "stored" is null (row is None) or a JSON object (row[0] text). */
            json_t *sj = json_obj_get(in, "stored");
            char *stored_str = NULL;
            if (sj && sj->type == JSON_OBJECT) stored_str = json_dumps(sj, 0);

            char root_buf[4096];
            const char *root = locmig_legacy_lineage_root(
                json_get_str(in, "session_id", ""),
                json_get_str(in, "profile_id", ""),
                json_get_str(in, "principal_id", ""),
                has_legacy, legacy, stored_str,
                root_buf, sizeof(root_buf), &refused);
            free(stored_str);

            out = refused ? json_null() : json_string(root ? root : "");
        } else {
            out = json_string("UNKNOWN_FUNC");
        }

        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        fputs(",", stdout);
        emit_key("refused", json_bool(refused ? true : false));
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
