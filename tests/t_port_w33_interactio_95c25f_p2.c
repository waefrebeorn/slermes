/* JSON-lines harness for the w33_interactio_95c25f_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w33_interactio_95c25f_p2.h"

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
        if (!strcmp(func, "usage")) {
            out = w33_interactio_95c25f_p2_usage(json_obj_get(in, "usage"));
        } else if (!strcmp(func, "merge_usage")) {
            out = w33_interactio_95c25f_p2_merge_usage(json_obj_get(in, "usages"));
        } else if (!strcmp(func, "finish_reason")) {
            out = w33_interactio_95c25f_p2_finish_reason(in);
        } else if (!strcmp(func, "interaction_error")) {
            out = w33_interactio_95c25f_p2_interaction_error(in);
        } else if (!strcmp(func, "translate")) {
            out = w33_interactio_95c25f_p2_translate(in);
        } else if (!strcmp(func, "stream")) {
            out = w33_interactio_95c25f_p2_stream(in);
        } else if (!strcmp(func, "event_token")) {
            out = w33_interactio_95c25f_p2_event_token(json_obj_get(in, "event"));
        } else if (!strcmp(func, "finish_ceiling")) {
            out = w33_interactio_95c25f_p2_finish_ceiling(in);
        } else if (!strcmp(func, "iter_events")) {
            out = w33_interactio_95c25f_p2_iter_events(json_obj_get(in, "lines"));
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
