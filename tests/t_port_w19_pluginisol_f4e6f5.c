#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_pluginisol_f4e6f5.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *take_str(char *s) {
    json_t *out = s ? json_string(s) : json_null();
    free(s);
    return out;
}

static long long num_field(json_t *in, const char *key, long long def) {
    json_t *v = json_obj_get(in, key);
    if (!v || v->type != JSON_NUMBER) return def;
    return (long long)v->num_val;
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
        if (!strcmp(func, "hermes_module")) {
            out = json_bool(w19_pluginisol_f4e6f5_hermes_module(json_get_str(in, "module", "")) != 0);
        } else if (!strcmp(func, "dedupe")) {
            out = w19_pluginisol_f4e6f5_dedupe(json_obj_get(in, "items"));
        } else if (!strcmp(func, "python_files")) {
            out = w19_pluginisol_f4e6f5_python_files(json_obj_get(in, "paths"));
        } else if (!strcmp(func, "ctx_method")) {
            out = w19_pluginisol_f4e6f5_ctx_method(json_get_str(in, "name", ""),
                                                   json_get_str(in, "rel", ""),
                                                   num_field(in, "line", 0));
        } else if (!strcmp(func, "hook_note")) {
            out = take_str(w19_pluginisol_f4e6f5_hook_note(json_get_str(in, "hook", "")));
        } else if (!strcmp(func, "registry_call")) {
            json_t *mp = json_obj_get(in, "model_provider");
            int mprov = (mp && mp->type == JSON_BOOL) ? mp->bool_val : 0;
            out = take_str(w19_pluginisol_f4e6f5_registry_reason(
                json_get_str(in, "target", ""), mprov,
                json_get_str(in, "rel", ""), num_field(in, "line", 0)));
        } else if (!strcmp(func, "patch_reason")) {
            out = take_str(w19_pluginisol_f4e6f5_patch_reason(
                json_get_str(in, "rel", ""), num_field(in, "line", 0),
                json_get_str(in, "owner", ""), json_get_str(in, "attr", "")));
        } else if (!strcmp(func, "setattr_reason")) {
            out = take_str(w19_pluginisol_f4e6f5_setattr_reason(
                json_get_str(in, "rel", ""), num_field(in, "line", 0),
                json_get_str(in, "owner", "")));
        } else if (!strcmp(func, "manifest_kind_reason")) {
            out = take_str(w19_pluginisol_f4e6f5_manifest_kind_reason(json_get_str(in, "kind", "")));
        } else if (!strcmp(func, "streaming_marker")) {
            const char *m = w19_pluginisol_f4e6f5_streaming_marker(json_get_str(in, "source", ""));
            out = m ? json_string(m) : json_null();
        } else if (!strcmp(func, "report")) {
            out = w19_pluginisol_f4e6f5_report(json_get_str(in, "verdict", ""),
                                               json_obj_get(in, "reasons"),
                                               json_obj_get(in, "notes"),
                                               json_obj_get(in, "ctx_methods"));
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
