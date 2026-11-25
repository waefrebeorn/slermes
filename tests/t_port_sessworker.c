#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_sessworker.h"

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
        if (!strcmp(func, "acquire")) {
            const char *process = json_get_str(in, "process", NULL);
            int cancel = json_is_true(json_obj_get(in, "cancel")) ? 1 : 0;
            sessworker_outcome_t o = sessworker_acquire_process(process, cancel);
            out = json_object();
            json_set(out, "cancelled", json_bool(o.cancelled ? 1 : 0));
            json_set(out, "returned", o.returned ? json_string(o.returned) : json_null());
            json_t *closed = json_array();
            if (o.closed)
                json_append(closed, o.closed_process ? json_string(o.closed_process) : json_null());
            json_set(out, "closed", closed);
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
