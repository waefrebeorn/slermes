/* JSON-lines harness for the w28_adapter_ca7894_p3 gate (email adapter
 * authentication verdict and the dispatch derivations).
 *
 * One request object per stdin line: {"func": <name>, ...inputs...}.
 * One response line per request: {"func", "out", <every input key echoed>}.
 * Output is unbuffered so the oracle can stream both sides. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w28_adapter_ca7894_p3.h"

typedef json_t *(*w_fn)(const json_t *);

static const struct { const char *name; w_fn fn; } W_TABLE[] = {
    { "verify_sender_auth",   w28_adapter_ca7894_verify_sender_auth },
    { "sender_accepted",      w28_adapter_ca7894_sender_accepted },
    { "trim_seen_uids",       w28_adapter_ca7894_trim_seen_uids },
    { "message_id_domain",    w28_adapter_ca7894_message_id_domain },
    { "reply_threading",      w28_adapter_ca7894_reply_threading },
    { "attachment_meta",      w28_adapter_ca7894_attachment_meta },
};;

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[1048576];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        size_t i;
        int matched = 0;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        for (i = 0; i < sizeof(W_TABLE) / sizeof(W_TABLE[0]); i++) {
            if (!strcmp(func, W_TABLE[i].name)) {
                out = W_TABLE[i].fn(in);
                matched = 1;
                break;
            }
        }
        if (!matched) out = json_string("UNKNOWN_FUNC");
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (i = 0; i < json_object_size(in); i++) {
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
