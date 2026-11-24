/* JSON-lines harness for the w25_usage_61b537 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_usage_61b537.h"

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
        size_t i;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "snapshot_document")) {
            out = w25_usage_61b537_snapshot_document(json_obj_get(in, "snapshot"));
        } else if (!strcmp(func, "no_usage_message")) {
            out = w25_usage_61b537_no_usage_message(json_obj_get(in, "provider"));
        } else if (!strcmp(func, "constants")) {
            out = w25_usage_61b537_constants();
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        {
            json_t *f = json_string(func);
            emit_key("func", f);
            json_free(f);
        }
        fputs(",", stdout);
        emit_key("out", out);
        for (i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            json_t *v;
            if (!k || !strcmp(k, "func")) continue;
            v = json_copy(json_object_get_at(in, i));
            fputs(",", stdout);
            emit_key(k, v);
            json_free(v);
        }
        fputs("}\n", stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
