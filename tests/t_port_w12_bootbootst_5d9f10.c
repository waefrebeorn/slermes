#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_bootbootst_5d9f10.h"

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
        if (!strcmp(func, "owed_config_migration")) {
            out = w12_bootbootst_5d9f10_owed_config_migration(json_obj_get(in, "known"));
        } else if (!strcmp(func, "owed_steps")) {
            out = w12_bootbootst_5d9f10_owed_steps(json_obj_get(in, "known"));
        } else if (!strcmp(func, "read_last_known")) {
            out = w12_bootbootst_5d9f10_read_last_known(json_get_str(in, "text", ""));
        } else if (!strcmp(func, "current_install_identity")) {
            out = w12_bootbootst_5d9f10_current_install_identity(
                    json_get_bool(in, "has_git", false) ? 1 : 0,
                    json_obj_get(in, "git_head"), json_obj_get(in, "stamp"));
        } else if (!strcmp(func, "needs_bootstrap")) {
            out = w12_bootbootst_5d9f10_needs_bootstrap(
                    json_obj_get(in, "identity"), json_obj_get(in, "known"));
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
