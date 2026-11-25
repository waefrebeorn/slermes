/* t_port_w5_progress_88711f.c — harness for pm/progress.py port.
 * Reads JSON lines {"func":<name>, ...args...}, emits
 * {"func":<name>,"out":<value>, ...all other input keys echoed...}.
 * Gate: run_one_oracle.sh w5_progress_88711f oracle_w5_progress_88711f.py "" \
 *       "src/pm/progress.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_progress_88711f.h"

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
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "w5_progress_88711f_verbose_output")) {
            out = json_bool(w5_progress_88711f_verbose_output(json_obj_get(in, "env")));
        } else if (!strcmp(func, "w5_progress_88711f_ansi_strip")) {
            char *r = w5_progress_88711f_ansi_strip(json_get_str(in, "line", ""));
            out = json_string(r ? r : "");
            free(r);
        } else if (!strcmp(func, "w5_progress_88711f_normalize_lines")) {
            out = w5_progress_88711f_normalize_lines(json_get_str(in, "partial", ""),
                                                     json_get_str(in, "text", ""));
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
