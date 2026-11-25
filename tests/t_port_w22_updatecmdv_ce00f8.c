/* JSON-lines harness for the w22_updatecmdv_ce00f8 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_updatecmdv_ce00f8.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[131072];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func, *mode, *stdout_text, *rc;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        mode = json_get_str(in, "mode", "ok");
        stdout_text = json_get_str(in, "stdout", NULL);
        rc = json_get_str(in, "rc", NULL);
        if (!strcmp(func, "failures")) {
            if (!strcmp(mode, "timeout")) out = w22_updatecmdv_ce00f8_probe_timeout();
            else if (!strcmp(mode, "oserror")) out = json_object();
            else out = w22_updatecmdv_ce00f8_failures(stdout_text, rc);
        } else if (!strcmp(func, "validate")) {
            if (!strcmp(mode, "timeout")) {
                json_t *f = w22_updatecmdv_ce00f8_probe_timeout();
                out = json_array();
                json_append(out, json_bool(0));
                json_append(out, json_string(json_object_get_key_at(f, 0)));
                json_append(out, json_string(json_get(json_object_get_at(f, 0), 1)->str_val));
                json_free(f);
            } else if (!strcmp(mode, "oserror")) {
                out = json_array();
                json_append(out, json_bool(1));
                json_append(out, json_null());
                json_append(out, json_null());
            } else {
                out = w22_updatecmdv_ce00f8_validate(stdout_text, rc);
            }
        } else if (!strcmp(func, "probe_failure")) {
            out = w22_updatecmdv_ce00f8_probe_failure(json_get_str(in, "kind", NULL),
                                                     json_get_str(in, "detail", NULL));
        } else if (!strcmp(func, "modules")) {
            int i;
            out = json_array();
            for (i = 0; W22_UPDATECMDV_CE00F8_MODULES[i]; i++) {
                json_append(out, json_string(W22_UPDATECMDV_CE00F8_MODULES[i]));
            }
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
