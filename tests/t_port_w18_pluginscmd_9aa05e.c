#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_pluginscmd_9aa05e.h"

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
        char *tmp = NULL;
        if (!strcmp(func, "looks")) {
            out = json_bool(pluginscmd_looks_like_catalog_name(json_get_str(in, "identifier", "")) ? 1 : 0);
        } else if (!strcmp(func, "pin")) {
            out = json_bool(pluginscmd_at_catalog_pin(json_obj_get(in, "sidecar"),
                                                      json_get_str(in, "entry_sha", "")) ? 1 : 0);
        } else if (!strcmp(func, "platforms")) {
            out = pluginscmd_normalized_platforms(json_obj_get(in, "platforms"));
        } else if (!strcmp(func, "added")) {
            out = pluginscmd_added(json_get_str(in, "surface", ""),
                                   json_obj_get(in, "old_set"), json_obj_get(in, "new_set"));
        } else if (!strcmp(func, "delta")) {
            out = pluginscmd_surface_delta(json_obj_get(in, "old_map"), json_obj_get(in, "new_map"));
        } else if (!strcmp(func, "delta_lines")) {
            out = pluginscmd_surface_delta_lines(json_obj_get(in, "delta"));
        } else if (!strcmp(func, "pin_label")) {
            tmp = pluginscmd_pin_label(json_get_str(in, "version", ""), json_get_str(in, "sha", ""));
        }
        if (tmp) { out = json_string(tmp); free(tmp); }
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
