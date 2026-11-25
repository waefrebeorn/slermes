/* JSON-lines harness for the w20_localmigra_5dcdb5 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_localmigra_5dcdb5.h"

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
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "bind_legacy_target")) {
            out = w20_localmigra_5dcdb5_bind_legacy_target(json_obj_get(in, "receipt"),
                                                           json_obj_get(in, "scene"));
        } else if (!strcmp(func, "canonical_dumps")) {
            out = w20_localmigra_5dcdb5_canonical_dumps(json_obj_get(in, "value"));
        } else if (!strcmp(func, "canonical_json")) {
            out = w20_localmigra_5dcdb5_canonical_json(json_obj_get(in, "value"));
        } else if (!strcmp(func, "binding_prefix")) {
            out = json_string(w20_localmigra_5dcdb5_binding_prefix());
        } else if (!strcmp(func, "legacy_prefix")) {
            out = json_string(w20_localmigra_5dcdb5_legacy_prefix());
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
