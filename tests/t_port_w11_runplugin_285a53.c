#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_runplugin_285a53.h"

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
        if (!strcmp(func, "count")) {
            json_t *pv = json_obj_get(in, "profile_name");
            const char *pn = (pv && pv->type == JSON_STRING) ? pv->str_val : NULL;
            int n = w11_runplugin_285a53_count(json_obj_get(in, "adapters"),
                        json_obj_get(in, "profile_adapters"), pn);
            out = json_number((double)n);
        } else if (!strcmp(func, "rewire_count")) {
            json_t *pv = json_obj_get(in, "profile_name");
            const char *pn = (pv && pv->type == JSON_STRING) ? pv->str_val : NULL;
            int n = w11_runplugin_285a53_rewire_count(json_obj_get(in, "adapters"),
                        json_obj_get(in, "profile_adapters"), pn, json_obj_get(in, "fail"));
            out = json_number((double)n);
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
