/* JSON-lines harness for the w25_devices_9a1f72 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_devices_9a1f72.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "probe_devices")) {
            out = w25_devices_9a1f72_probe_devices(in);
        } else if (!strcmp(func, "library")) {
            out = w25_devices_9a1f72_library(in);
        } else if (!strcmp(func, "constants")) {
            out = w25_devices_9a1f72_constants();
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            json_t *copy;
            if (!k || !strcmp(k, "func")) continue;
            copy = json_copy(json_object_get_at(in, i));
            fputs(",", stdout);
            emit_key(k, copy);
            json_free(copy);
        }
        fputs("}\n", stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
