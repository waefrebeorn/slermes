#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_modelslocal_6756db.h"

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
        if (!strcmp(func, "normalize")) {
            out = w9_modelslocal_6756db_normalize_openai_base_url(json_get_str(in, "base_url", ""));
        } else if (!strcmp(func, "root")) {
            out = w9_modelslocal_6756db_root_for_ollama_native_api(json_get_str(in, "base_url", ""));
        } else if (!strcmp(func, "host")) {
            out = w9_modelslocal_6756db_ollama_host_from_env(json_get_str(in, "env_host", ""));
        } else if (!strcmp(func, "suffix")) {
            out = w9_modelslocal_6756db_strip_ollama_cloud_suffix(json_get_str(in, "model_id", ""));
        } else if (!strcmp(func, "pint")) {
            out = w9_modelslocal_6756db_positive_int(json_obj_get(in, "value"));
        } else if (!strcmp(func, "tags")) {
            out = w9_modelslocal_6756db_parse_ollama_tags(json_obj_get(in, "payload"));
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
