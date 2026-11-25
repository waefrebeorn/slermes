/* JSON-lines harness for the w30_messages_29cf8f_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w30_messages_29cf8f_p2.h"

typedef json_t *(*msg_fn)(const json_t *);

static const struct {
    const char *name;
    msg_fn fn;
} TABLE[] = {
    {"message_record", w30_messages_29cf8f_p2_message_record},
    {"stamp", w30_messages_29cf8f_p2_stamp},
    {"append_chunk", w30_messages_29cf8f_p2_append_chunk},
    {"row_outcomes", w30_messages_29cf8f_p2_row_outcomes},
    {"batch_chunks", w30_messages_29cf8f_p2_batch_chunks},
    {"replace_rows", w30_messages_29cf8f_p2_replace_rows},
    {"reactions", w30_messages_29cf8f_p2_reactions},
    {"unseen_reactions", w30_messages_29cf8f_p2_unseen_reactions},
    {"cooldown_snapshot", w30_messages_29cf8f_p2_cooldown_snapshot},
    {"compression_cooldown", w30_messages_29cf8f_p2_compression_cooldown},
    {"storage_state", w30_messages_29cf8f_p2_storage_state},
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
