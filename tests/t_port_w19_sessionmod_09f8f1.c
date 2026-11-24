#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_sessionmod_09f8f1.h"

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
        if (!strcmp(func, "display_base_url")) {
            const char *value = json_get_str(in, "value", "");
            char *r = w19_sessionmod_09f8f1_display_base_url(value);
            out = r ? json_string(r) : json_null();
            free(r);
        } else if (!strcmp(func, "resolve_settings")) {
            json_t *model_cfg = json_obj_get(in, "model_cfg");
            json_t *override = json_obj_get(in, "override");
            json_t *store_override = json_obj_get(in, "store_override");
            json_t *hc = json_obj_get(in, "has_channel");
            int has_channel = (hc && hc->type == JSON_BOOL) ? hc->bool_val : 0;
            out = w19_sessionmod_09f8f1_resolve_settings(
                model_cfg, json_get_str(in, "key", ""), override, store_override, has_channel,
                json_get_str(in, "channel_model", ""), json_get_str(in, "channel_provider", ""));
        } else if (!strcmp(func, "settings_display")) {
            out = w19_sessionmod_09f8f1_settings_display(json_obj_get(in, "settings"));
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
