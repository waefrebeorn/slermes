#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_onfacts.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static const char *nullable_str(const json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    if (!v || v->type == JSON_NULL) return NULL;
    return v->type == JSON_STRING ? v->str_val : "";
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

        if (!strcmp(func, "read_facts")) {
            onfacts_inputs_t in_data;
            memset(&in_data, 0, sizeof(in_data));
            in_data.os_family = json_get_str(in, "os_family", "");
            in_data.native_arch = json_get_str(in, "native_arch", "");
            in_data.machine_kind = json_get_str(in, "machine_kind", "");
            in_data.nvidia_platform = nullable_str(in, "nvidia_platform");
            in_data.host_locale = nullable_str(in, "host_locale");
            in_data.facts_source = json_get_str(in, "facts_source", "");
            in_data.locale = nullable_str(in, "locale");
            in_data.tours = json_get_bool(in, "tours", false) ? 1 : 0;
            json_t *plugs = json_obj_get(in, "plugins");
            in_data.plugin_count = json_is_array(plugs) ? json_len(plugs) : 0;
            const char **parr = in_data.plugin_count
                ? (const char **)malloc(in_data.plugin_count * sizeof(char *)) : NULL;
            for (size_t i = 0; i < in_data.plugin_count; i++)
                parr[i] = json_string_value(json_get(plugs, i));
            in_data.plugins = parr;

            onfacts_facts_t facts;
            onfacts_read_facts(&in_data, &facts);
            out = json_object();
            json_set(out, "os", json_string(facts.os));
            json_set(out, "arch", json_string(facts.arch));
            json_set(out, "machine_kind", json_string(facts.machine_kind));
            json_set(out, "nvidia_platform",
                     facts.nvidia_platform ? json_string(facts.nvidia_platform) : json_null());
            json_set(out, "facts_source", json_string(facts.facts_source));
            json_set(out, "locale", facts.locale ? json_string(facts.locale) : json_null());
            json_t *jplugs = json_array();
            for (size_t i = 0; i < facts.plugin_count; i++)
                json_append(jplugs, json_string(facts.plugins[i]));
            json_set(out, "plugins", jplugs);
            json_set(out, "tours", json_bool(facts.tours));
            onfacts_facts_free(&facts);
            free(parr);
        } else if (!strcmp(func, "suggested_name")) {
            const char *fn = nullable_str(in, "full_name");
            char *s = onfacts_suggested_name(fn);
            out = s ? json_string(s) : json_null();
            free(s);
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
