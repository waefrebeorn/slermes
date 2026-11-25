/* JSON-lines harness for the w30_messages_29cf8f_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w30_messages_29cf8f_p1.h"

typedef json_t *(*msg_fn)(const json_t *);

static const struct {
    const char *name;
    msg_fn fn;
} TABLE[] = {
    {"rewind_refusal_reason", w30_messages_29cf8f_p1_rewind_refusal_reason},
    {"visibility", w30_messages_29cf8f_p1_visibility},
    {"message_dict", w30_messages_29cf8f_p1_message_dict},
    {"held_dict", w30_messages_29cf8f_p1_held_dict},
    {"pruned_original", w30_messages_29cf8f_p1_pruned_original},
    {"transcript_row", w30_messages_29cf8f_p1_transcript_row},
    {"sidecar", w30_messages_29cf8f_p1_sidecar},
    {"wire_fields", w30_messages_29cf8f_p1_wire_fields},
};

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
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        for (size_t i = 0; i < sizeof(TABLE) / sizeof(TABLE[0]); i++) {
            if (!strcmp(func, TABLE[i].name)) { out = TABLE[i].fn(in); break; }
        }
        if (!out) out = json_string("UNKNOWN_FUNC");
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
