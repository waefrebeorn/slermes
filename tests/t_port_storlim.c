/* t_port_storlim.c — differential harness for gate storlim.
 * Reads {"func":"resume"|"export"|"limit","sessions":<json>,...},
 * emits {"func":...,"out":"<decimal int>",...echoed input keys}.
 * Gate: run_one_oracle.sh storlim oracle_storlim.py "" \
 *   "src/storage_limits.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_storlim.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static json_t *num_str(int64_t v) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%lld", (long long)v);
    return json_string(buf);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *sessions = json_obj_get(in, "sessions");
        json_t *out = NULL;
        if (!strcmp(func, "resume")) {
            out = num_str(storlim_resolved_max_resume_messages(sessions));
        } else if (!strcmp(func, "export")) {
            out = num_str(storlim_resolved_max_export_messages(sessions));
        } else if (!strcmp(func, "limit")) {
            int64_t fallback = (int64_t)strtoll(json_get_str(in, "fallback", "20000"), NULL, 10);
            out = num_str(storlim_configured_transcript_limit(
                json_get_str(in, "key", ""), sessions, fallback));
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
