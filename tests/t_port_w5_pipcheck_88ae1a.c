#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_pipcheck_88ae1a.h"

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
        if (!strcmp(func, "normalize")) {
            out = w5_pipcheck_88ae1a_normalize(json_get_str(in, "name", ""));
        } else if (!strcmp(func, "override_requirements")) {
            out = w5_pipcheck_88ae1a_override_requirements(json_obj_get(in, "entries"));
        } else if (!strcmp(func, "overridden_versions")) {
            out = w5_pipcheck_88ae1a_overridden_versions(json_obj_get(in, "entries"),
                                                         json_obj_get(in, "packages"));
        } else if (!strcmp(func, "unexpected")) {
            out = w5_pipcheck_88ae1a_unexpected(json_get_str(in, "report", ""),
                                                json_obj_get(in, "lock"));
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
