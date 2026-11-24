/* t_port_w19_client_ead47a.c - harness for src/plugins/model-providers/gemini/client.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_client_ead47a.h"

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
        int config_ok = json_get_bool(in, "config_unreadable", false) ? 0 : 1;
        json_t *out = NULL;
        if (!strcmp(func, "bare_model_id")) {
            out = json_string(w19_client_ead47a_bare_model_id(json_obj_get(in, "model")
                                && json_obj_get(in, "model")->type == JSON_STRING
                                ? json_obj_get(in, "model")->str_val : ""));
        } else if (!strcmp(func, "uses_thinking_level")) {
            out = json_bool(w19_client_ead47a_uses_thinking_level(
                    json_obj_get(in, "model") && json_obj_get(in, "model")->type == JSON_STRING
                    ? json_obj_get(in, "model")->str_val : "") ? true : false);
        } else if (!strcmp(func, "configured_api")) {
            const char *model = json_get_str(in, "model", "");
            const char *bare = json_get_str(in, "bare", "");
            out = json_string(w19_client_ead47a_configured_api(model, bare, config_ok,
                                                               json_obj_get(in, "config")));
        } else if (!strcmp(func, "uses_interactions")) {
            const char *model = json_get_str(in, "model", "");
            const char *base_url = json_get_str(in, "base_url", "");
            out = json_bool(w19_client_ead47a_uses_interactions(model, base_url, config_ok,
                                                                json_obj_get(in, "config")) ? true : false);
        } else if (!strcmp(func, "continues")) {
            out = json_bool(w19_client_ead47a_continues(json_obj_get(in, "interaction")) ? true : false);
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
