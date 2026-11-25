#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w17_pluginkeyl_d5f537.h"

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
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        const json_t *src = json_obj_get(in, "source");
        int has_source = json_is_object(src) ? 1 : 0;
        const json_t *plat = has_source ? json_obj_get(src, "platform") : NULL;
        int platform_is_local = (plat && plat->type == JSON_STRING && !strcmp(plat->str_val, "local")) ? 1 : 0;
        const json_t *psrc = has_source ? json_obj_get(src, "policy_source") : NULL;
        int has_policy = (psrc && psrc->type == JSON_STRING) ? 1 : 0;
        if (!strcmp(func, "local")) {
            out = json_bool(w17_pluginkeyl_d5f537_local(has_source, platform_is_local,
                (has_source && json_get_bool(src, "authorized", false)) ? 1 : 0) ? true : false);
        } else if (!strcmp(func, "launched")) {
            out = json_bool(w17_pluginkeyl_d5f537_launched_by_classic_cli(
                (has_source && json_get_bool(src, "policy_refusal", false)) ? 1 : 0,
                has_policy, has_policy ? psrc->str_val : "") ? true : false);
        } else if (!strcmp(func, "resolve")) {
            out = w17_pluginkeyl_d5f537_resolve(in);
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
