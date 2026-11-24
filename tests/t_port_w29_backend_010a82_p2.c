/* JSON-lines harness for the w29_backend_010a82_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_backend_010a82_p2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "install")) {
            out = w29_backend_010a82_install(in);
        } else if (!strcmp(func, "diff_base")) {
            out = w29_backend_010a82_diff_base(json_obj_get(in, "state"));
        } else if (!strcmp(func, "version_of")) {
            out = w29_backend_010a82_version_of(json_obj_get(in, "state"));
        } else if (!strcmp(func, "cache_version")) {
            out = w29_backend_010a82_cache_version(json_obj_get(in, "version"));
        } else if (!strcmp(func, "exists")) {
            out = w29_backend_010a82_exists();
        } else if (!strcmp(func, "locked")) {
            out = w29_backend_010a82_locked(in);
        } else if (!strcmp(func, "first_change")) {
            out = w29_backend_010a82_first_change(in);
        } else if (!strcmp(func, "inherited_notes")) {
            out = w29_backend_010a82_inherited_notes(in);
        } else if (!strcmp(func, "migration_check_input")) {
            out = w29_backend_010a82_migration_check_input(in);
        } else if (!strcmp(func, "describe")) {
            out = w29_backend_010a82_describe(in);
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
