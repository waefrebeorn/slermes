/* JSON-lines harness for the w20_audiobridg_4173c4 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_audiobridg_4173c4.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *str_or(json_t *in, const char *key, const char *def) {
    return json_get_str(in, key, def);
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
        if (!strcmp(func, "parse_module_id")) {
            out = w20_audiobridg_4173c4_parse_module_id(str_or(in, "stdout", ""));
        } else if (!strcmp(func, "setup")) {
            out = w20_audiobridg_4173c4_setup(str_or(in, "system", ""),
                                              json_obj_get(in, "script"));
        } else if (!strcmp(func, "teardown")) {
            out = w20_audiobridg_4173c4_teardown(json_obj_get(in, "module_ids"));
        } else if (!strcmp(func, "finish")) {
            out = w20_audiobridg_4173c4_finish(str_or(in, "tag", ""),
                                               str_or(in, "device", ""),
                                               str_or(in, "write_target", ""),
                                               json_obj_get(in, "module_ids"));
        } else if (!strcmp(func, "ready")) {
            out = w20_audiobridg_4173c4_ready(json_obj_get(in, "value"));
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
