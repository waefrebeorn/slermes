/* JSON-lines harness for the w23_sessionuna_84b661 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_sessionuna_84b661.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
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
        if (!strcmp(func, "is_unattended")) {
            out = json_bool(w23_sessionuna_84b661_is_unattended(json_obj_get(in, "row")));
        } else if (!strcmp(func, "unattended_unknown")) {
            out = w23_sessionuna_84b661_unattended_unknown(json_obj_get(in, "records"));
        } else if (!strcmp(func, "settle_recovered")) {
            out = w23_sessionuna_84b661_settle_recovered(in);
        } else if (!strcmp(func, "settle_drain")) {
            out = w23_sessionuna_84b661_settle_drain(in);
        } else if (!strcmp(func, "cron_principal")) {
            out = json_string(w23_sessionuna_84b661_cron_principal());
        } else if (!strcmp(func, "notice")) {
            out = json_string(w23_sessionuna_84b661_notice());
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
