/* JSON-lines harness for the w22_sessionscm_632617 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_sessionscm_632617.h"

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
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "kind_label")) {
            const char *label = w22_sessionscm_632617_kind_label(json_get_str(in, "kind", ""));
            out = json_string(label ? label : "");
        } else if (!strcmp(func, "group")) {
            out = w22_sessionscm_632617_group(json_obj_get(in, "findings"));
        } else if (!strcmp(func, "report")) {
            char *text = w22_sessionscm_632617_report(json_obj_get(in, "findings"));
            out = text ? json_string(text) : json_null();
            free(text);
        } else if (!strcmp(func, "run")) {
            out = w22_sessionscm_632617_run(in);
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
