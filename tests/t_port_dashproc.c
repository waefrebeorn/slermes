/* t_port_dashproc.c — differential harness for gate dashproc.
 * Reads {"func":"index"|"ephemeral","argv":[...]}, emits
 * {"func":...,"out":int|null|bool,...echoed input keys}.
 * Gate: run_one_oracle.sh dashproc oracle_dashproc.py "" \
 *   "src/persistence/sqlite/_dashboard_procs.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_dashproc.h"

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
        json_t *argv = json_obj_get(in, "argv");
        json_t *out = NULL;
        if (!strcmp(func, "index")) {
            int idx = dashproc_dashboard_subcommand_index(argv);
            out = idx < 0 ? json_null() : json_number((double)idx);
        } else if (!strcmp(func, "ephemeral")) {
            out = json_bool((bool)dashproc_is_ephemeral_port_zero_backend(argv));
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
