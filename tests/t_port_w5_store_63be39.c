#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_store_63be39.h"

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

        if (!strcmp(func, "entry_component")) {
            char *name = NULL;
            if (w5_store_63be39_entry_component(json_get_str(in, "name", ""), &name)) {
                out = json_string(name);
                w5_store_63be39_free(name);
            } else out = json_null();
        } else if (!strcmp(func, "fetch_entry_name")) {
            char *name = NULL;
            if (w5_store_63be39_fetch_entry_name(json_get_str(in, "digest", ""), &name)) {
                out = json_string(name);
                w5_store_63be39_free(name);
            } else out = json_null();
        } else if (!strcmp(func, "current_target")) {
            char *t = w5_store_63be39_current_target(
                json_get_str(in, "machine", ""),
                json_get_str(in, "sys_platform", ""),
                json_get_bool(in, "bionic", 0),
                json_get_bool(in, "musl", 0));
            out = t ? json_string(t) : json_null();
            w5_store_63be39_free(t);
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
