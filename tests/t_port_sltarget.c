/* t_port_sltarget.c — differential harness for gate sltarget.
 * Reads {"func":"parse","ref":...} lines, emits {"func":...,"out":[chat,thread]|null,
 * ...echoed input keys}. Gate: run_one_oracle.sh sltarget oracle_sltarget.py "" \
 *   "src/plugins/platforms/slack/send_targets.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_sltarget.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "parse")) {
            char *chat = NULL, *thread = NULL;
            if (sltarget_parse_send_target(json_get_str(in, "ref", ""), &chat, &thread)) {
                out = json_array();
                json_append(out, json_string(chat));
                json_append(out, thread ? json_string(thread) : json_null());
                free(chat); free(thread);
            } else {
                out = json_null();
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
        json_free(out);
        json_free(in);
    }
    return 0;
}
