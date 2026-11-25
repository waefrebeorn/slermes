/* JSON-lines harness for the w27_adapter_97bbcb_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_97bbcb_p1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[1048576];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "extract_message_payload")) {
            out = w27_gchat_p1_extract_message_payload(in);
        } else if (!strcmp(func, "prepare_inbound")) {
            out = w27_gchat_p1_prepare_inbound(in);
        } else if (!strcmp(func, "thread_body")) {
            out = w27_gchat_p1_thread_body(in);
        } else if (!strcmp(func, "create_kwargs")) {
            out = w27_gchat_p1_create_kwargs(in);
        } else if (!strcmp(func, "mime_for_message_type")) {
            out = w27_gchat_p1_mime_for_message_type(in);
        } else if (!strcmp(func, "route_pubsub_event")) {
            out = w27_gchat_p1_route_pubsub_event(in);
        } else if (!strcmp(func, "build_message_event")) {
            out = w27_gchat_p1_build_message_event(in);
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
