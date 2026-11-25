#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w17_configrout_51eafb.h"

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
        json_t *a = json_obj_get(in, "in");
        json_t *out = NULL;
        if (!strcmp(func, "route_of"))
            out = w17_configrout_51eafb_route_of(json_get_str(a, "provider", ""),
                                                  json_get_str(a, "base_url", ""));
        else if (!strcmp(func, "url"))
            out = w17_configrout_51eafb_url(json_get_str(a, "value", ""));
        else if (!strcmp(func, "clear_aux_credentials"))
            out = w17_configrout_51eafb_clear_aux_credentials(json_obj_get(a, "entry"),
                      json_get_str(a, "provider", ""), json_get_str(a, "base_url", ""));
        else out = json_string("UNKNOWN_FUNC");
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
