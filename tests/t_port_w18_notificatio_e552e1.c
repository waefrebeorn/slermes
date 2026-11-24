#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_notificatio_e552e1.h"

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
        json_t *a = json_obj_get(in, "in");
        json_t *evt = a ? json_obj_get(a, "evt") : NULL;
        json_t *out = NULL;
        if (!strcmp(func, "parkable"))
            out = w18_notificatio_e552e1_parkable(evt);
        else if (!strcmp(func, "is_progress"))
            out = w18_notificatio_e552e1_is_progress(evt);
        else if (!strcmp(func, "key"))
            out = w18_notificatio_e552e1_key(evt);
        else if (!strcmp(func, "admit_locked"))
            out = w18_notificatio_e552e1_admit_locked(a ? json_obj_get(a, "state") : NULL, evt,
                                                       json_get_str(a, "key", ""));
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
        json_free(out);
        json_free(in);
    }
    return 0;
}
