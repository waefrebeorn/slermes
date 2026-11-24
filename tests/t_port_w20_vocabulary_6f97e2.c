/* JSON-lines harness for the w20_vocabulary_6f97e2 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_vocabulary_6f97e2.h"

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
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "fingerprint")) {
            out = w20_vocabulary_6f97e2_admission_fingerprint(json_get_str(in, "target", ""),
                                                              json_get_str(in, "payload_json", NULL));
        } else if (!strcmp(func, "queue_max")) {
            out = w20_vocabulary_6f97e2_queue_max();
        } else if (!strcmp(func, "retry_horizon")) {
            out = w20_vocabulary_6f97e2_retry_horizon();
        } else if (!strcmp(func, "admitted_request_conflict")) {
            out = w20_vocabulary_6f97e2_admitted_request_conflict();
        } else if (!strcmp(func, "classify")) {
            out = w20_vocabulary_6f97e2_classify_label(json_get_str(in, "label", NULL));
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
