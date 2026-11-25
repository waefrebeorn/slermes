/* JSON-lines harness for the w20_codeexecut_b10023 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_codeexecut_b10023.h"

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
        if (!strcmp(func, "is_inert_python")) {
            out = json_bool(w20_codeexecut_b10023_is_inert_python(str_or(in, "code", "")) ? 1 : 0);
        } else if (!strcmp(func, "rejection")) {
            char *msg = w20_codeexecut_b10023_rejection(
                str_or(in, "code", ""),
                json_get_bool(in, "contains", 1) ? 1 : 0,
                json_get_bool(in, "host_kill", 0) ? 1 : 0,
                str_or(in, "match", ""),
                str_or(in, "host_kill_text", ""));
            out = msg ? json_string(msg) : json_null();
            free(msg);
        } else if (!strcmp(func, "safe_builtin")) {
            out = json_bool(w20_codeexecut_b10023_safe_builtin(str_or(in, "name", "")) ? 1 : 0);
        } else if (!strcmp(func, "inert_import")) {
            out = json_bool(w20_codeexecut_b10023_inert_import(str_or(in, "name", "")) ? 1 : 0);
        } else if (!strcmp(func, "sink_attr")) {
            out = json_bool(w20_codeexecut_b10023_sink_attr(str_or(in, "name", "")) ? 1 : 0);
        } else if (!strcmp(func, "max_match_chars")) {
            out = json_number(w20_codeexecut_b10023_max_match_chars());
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
