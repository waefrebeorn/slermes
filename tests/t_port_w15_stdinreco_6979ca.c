/* Port gate harness for tui_gateway/_stdin_recovery.py (gate w15_stdinreco_6979ca). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_stdinreco_6979ca.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *jstr_or_null(json_t *in, const char *k) {
    json_t *v = json_obj_get(in, k);
    if (!v || v->type == JSON_NULL) return NULL;
    return v->type == JSON_STRING ? v->str_val : "";
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
        char *tmp = NULL;
        if (!strcmp(func, "diagnose")) {
            tmp = w15_stdinreco_6979ca_diagnose_stdin_state(
                json_get_bool(in, "fcntl_available", 0) ? 1 : 0,
                (long long)json_get_num(in, "flags", 0.0),
                (long long)json_get_num(in, "nonblock_flag", 2048.0),
                jstr_or_null(in, "fgetfl_error"), jstr_or_null(in, "rcvtimeout_repr"));
            out = json_string(tmp ? tmp : "");
        } else if (!strcmp(func, "handle")) {
            out = w15_stdinreco_6979ca_handle_spurious_eof(
                json_get_bool(in, "nonblock", 0) ? 1 : 0,
                json_obj_get(in, "recovery_times"),
                json_get_num(in, "now", 0.0),
                (int)json_get_num(in, "max_recoveries", 10.0),
                jstr_or_null(in, "diag_state"));
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
        free(tmp);
        json_free(out);
        json_free(in);
    }
    return 0;
}
