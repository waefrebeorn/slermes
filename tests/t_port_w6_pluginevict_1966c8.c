/* Port gate harness for pm/plugin_eviction.py (gate w6_pluginevict_1966c8). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_pluginevict_1966c8.h"

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
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "discard")) {
            out = pluginevict_discard(json_obj_get(in, "names"), json_obj_get(in, "spellings"));
        } else if (!strcmp(func, "list")) {
            out = pluginevict_list(json_obj_get(in, "plugins"), json_get_str(in, "key", "disabled"));
        } else if (!strcmp(func, "prune")) {
            out = pluginevict_prune(json_obj_get(in, "plugins"));
        } else if (!strcmp(func, "settle")) {
            out = pluginevict_settle(json_obj_get(in, "plugins"), json_obj_get(in, "settled"));
        } else if (!strcmp(func, "notices")) {
            out = pluginevict_notices(json_obj_get(in, "facts"));
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
