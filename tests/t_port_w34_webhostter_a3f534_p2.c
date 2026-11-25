/* JSON-lines harness for the w34_webhostter_a3f534_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w34_webhostter_a3f534_p2.h"

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
        if (!strcmp(func, "metadata_payload")) {
            out = w34_webhostter_a3f534_p2_metadata_payload(in);
        } else if (!strcmp(func, "closed_payload")) {
            out = w34_webhostter_a3f534_p2_closed_payload(in);
        } else if (!strcmp(func, "frame_payload")) {
            out = w34_webhostter_a3f534_p2_frame_payload(in);
        } else if (!strcmp(func, "frame_cost")) {
            out = w34_webhostter_a3f534_p2_frame_cost(in);
        } else if (!strcmp(func, "queue_full")) {
            out = w34_webhostter_a3f534_p2_queue_full(in);
        } else if (!strcmp(func, "input_runs")) {
            out = w34_webhostter_a3f534_p2_input_runs(in);
        } else if (!strcmp(func, "resize_undo")) {
            out = w34_webhostter_a3f534_p2_resize_undo(in);
        } else if (!strcmp(func, "admit_verdict")) {
            out = w34_webhostter_a3f534_p2_admit_verdict(in);
        } else if (!strcmp(func, "token_ok")) {
            out = w34_webhostter_a3f534_p2_token_ok(in);
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
