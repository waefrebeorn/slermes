#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w17_browsercre_9241f4.h"

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
        json_t *rec = json_obj_get(in, "recorded");
        int rec_null = (!rec || rec->type == JSON_NULL) ? 1 : 0;
        const char *recorded = (rec && rec->type == JSON_STRING) ? rec->str_val : "";
        const char *credential = json_get_str(in, "credential", "");
        if (!strcmp(func, "generation_of")) {
            out = w17_browsercre_9241f4_generation_of(credential);
        } else if (!strcmp(func, "current_if")) {
            out = w17_browsercre_9241f4_current_if(recorded, rec_null, credential);
        } else if (!strcmp(func, "is_current")) {
            out = w17_browsercre_9241f4_is_current(recorded, rec_null, credential);
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
