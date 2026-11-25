/* JSON-lines harness for the w22_toolscache_8cdd07 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_toolscache_8cdd07.h"

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
        if (!strcmp(func, "member_key")) {
            char *s = w22_toolscache_8cdd07_member_key(json_get_str(in, "token", NULL));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "slug_valid")) {
            out = json_number((double)w22_toolscache_8cdd07_slug_valid(
                json_get_str(in, "slug", NULL)));
        } else if (!strcmp(func, "cache_rel")) {
            char *s = w22_toolscache_8cdd07_cache_rel(
                json_get_str(in, "origin", NULL),
                json_get_str(in, "member_key", NULL),
                json_get_str(in, "slug", NULL));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "home_env")) {
            char *s = w22_toolscache_8cdd07_home_env();
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "read_plan")) {
            out = w22_toolscache_8cdd07_read_plan(in);
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
