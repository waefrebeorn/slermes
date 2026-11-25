#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_mainprovid_56bced.h"

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
        char *tmp = NULL;
        if (!strcmp(func, "short_url")) {
            tmp = mainprovid_short_url(json_get_str(in, "url", ""));
        } else if (!strcmp(func, "effort")) {
            tmp = mainprovid_aux_effort_word(json_obj_get(in, "cfg"));
        } else if (!strcmp(func, "format_aux")) {
            tmp = mainprovid_format_aux_current(json_obj_get(in, "cfg"));
        } else if (!strcmp(func, "delegation_cfg")) {
            out = mainprovid_delegation_cfg_as_task(json_obj_get(in, "cfg"));
        } else if (!strcmp(func, "auto_name")) {
            tmp = mainprovid_auto_provider_name(json_get_str(in, "url", ""));
        } else if (!strcmp(func, "api_key_value")) {
            tmp = mainprovid_api_key_config_value(json_obj_get(in, "info"),
                                                  json_get_str(in, "resolved", ""));
        } else if (!strcmp(func, "base_url_value")) {
            tmp = mainprovid_base_url_config_value(json_obj_get(in, "info"),
                                                   json_get_str(in, "resolved", ""));
        } else if (!strcmp(func, "stepfun")) {
            tmp = mainprovid_infer_stepfun_region(json_get_str(in, "url", ""));
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
