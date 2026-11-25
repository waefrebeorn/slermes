/* JSON-lines harness for the w26_hostedroom_f8aecd gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_hostedroom_f8aecd.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static int inum(const json_t *in, const char *key, int def) {
    const json_t *v = json_obj_get(in, key);
    if (!v || v->type != JSON_NUMBER) return def;
    return (int)v->num_val;
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "source_for")) {
            out = w26_hostedroom_f8aecd_source_for(json_get_str(in, "target_name", ""),
                                                   inum(in, "sibling_exists", 0));
        } else if (!strcmp(func, "handoff_id")) {
            out = w26_hostedroom_f8aecd_handoff_id(json_get_str(in, "local_id", ""));
        } else if (!strcmp(func, "import_plan")) {
            out = w26_hostedroom_f8aecd_import_plan(json_obj_get(in, "authorities"));
        } else if (!strcmp(func, "constants")) {
            out = w26_hostedroom_f8aecd_constants();
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
