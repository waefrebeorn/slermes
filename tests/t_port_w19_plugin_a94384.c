#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_plugin_a94384.h"

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
        const char *name = json_get_str(in, "name", "acme");
        const char *handle = json_get_str(in, "handle", NULL);
        json_t *plugin = json_obj_get(in, "plugin");
        json_t *out = NULL;
        if (!strcmp(func, "text")) {
            out = w19_plugin_a94384_text(json_obj_get(in, "value"));
        } else if (!strcmp(func, "materialize")) {
            out = w19_plugin_a94384_materialize(json_obj_get(in, "meta"));
        } else if (!strcmp(func, "own")) {
            out = json_bool(w19_plugin_a94384_owns(name, handle) ? true : false);
        } else if (!strcmp(func, "list_items")) {
            out = w19_plugin_a94384_list_items(name, plugin);
        } else if (!strcmp(func, "get_meta")) {
            out = w19_plugin_a94384_get_meta(name, plugin, handle);
        } else if (!strcmp(func, "resolve_password")) {
            out = w19_plugin_a94384_resolve_password(name, plugin, handle);
        } else if (!strcmp(func, "resolve_otp")) {
            out = w19_plugin_a94384_resolve_otp(name, plugin, handle);
        } else if (!strcmp(func, "resolve_secret")) {
            out = w19_plugin_a94384_resolve_secret(name, plugin, handle);
        } else if (!strcmp(func, "fill_context")) {
            out = w19_plugin_a94384_fill_context(name, plugin);
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
            emit_key(k, json_object_get_at(in, i));
        }
        fputs("}\n", stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
