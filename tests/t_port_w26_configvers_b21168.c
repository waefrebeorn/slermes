/* JSON-lines harness for the w26_configvers_b21168 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_configvers_b21168.h"

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
        if (!strcmp(func, "coerce")) {
            out = w26_configvers_b21168_coerce(json_obj_get(in, "value"));
        } else if (!strcmp(func, "read_stamp")) {
            out = w26_configvers_b21168_read_stamp(in);
        } else if (!strcmp(func, "check_version")) {
            out = w26_configvers_b21168_check_version(in);
        } else if (!strcmp(func, "newer_message")) {
            out = w26_configvers_b21168_newer_message(inum(in, "stamp", 0));
        } else if (!strcmp(func, "constants")) {
            out = w26_configvers_b21168_constants();
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
