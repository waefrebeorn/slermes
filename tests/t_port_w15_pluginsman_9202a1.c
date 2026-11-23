#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_pluginsman_9202a1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static json_t *wrap_str(char *s) {
    json_t *out;
    if (s) { out = json_string(s); free(s); }
    else out = json_null();
    return out;
}
int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "portable_slug")) {
            out = wrap_str(w15_pluginsman_9202a1_portable_slug(json_get_str(in, "key", "")));
        } else if (!strcmp(func, "portable_mcp_server_name")) {
            out = wrap_str(w15_pluginsman_9202a1_portable_mcp_server_name(
                json_get_str(in, "key", ""), json_get_str(in, "server", "")));
        } else if (!strcmp(func, "display_author")) {
            out = wrap_str(w15_pluginsman_9202a1_display_author(json_obj_get(in, "value")));
        } else if (!strcmp(func, "config_key_path")) {
            out = wrap_str(w15_pluginsman_9202a1_config_key_path(json_get_str(in, "item", "")));
        } else if (!strcmp(func, "manifest_config_keys")) {
            out = w15_pluginsman_9202a1_manifest_config_keys(json_obj_get(in, "data"));
        } else if (!strcmp(func, "detect_kind")) {
            const char *k = w15_pluginsman_9202a1_detect_kind_from_source(
                json_get_str(in, "source", ""));
            out = k ? json_string(k) : json_null();
        } else if (!strcmp(func, "version_satisfies")) {
            out = json_bool(w15_pluginsman_9202a1_version_satisfies(
                json_get_str(in, "spec", ""), json_get_str(in, "current", "")) ? true : false);
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
