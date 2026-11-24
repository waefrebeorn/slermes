/* JSON-lines harness for the w27_adapter_04abb5_p2 gate (Buzz adapter CLI
 * plumbing / send shaping).  One request object per stdin line; one response
 * line carrying "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_04abb5_p2.h"

typedef json_t *(*w_fn)(const json_t *);

static const struct { const char *name; w_fn fn; } W_TABLE[] = {
    { "member_candidates",   w27_adapter_04abb5_member_candidates },
    { "profile_name",        w27_adapter_04abb5_profile_name },
    { "mention_pubkeys",     w27_adapter_04abb5_mention_pubkeys },
    { "send_ladder",         w27_adapter_04abb5_send_ladder },
    { "send_result",         w27_adapter_04abb5_send_result },
    { "chat_info",           w27_adapter_04abb5_chat_info },
    { "websocket_url",       w27_adapter_04abb5_websocket_url },
    { "channel_subscription", w27_adapter_04abb5_channel_subscription },
    { "subscriptions",       w27_adapter_04abb5_subscriptions },
};

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
        json_t *in, *out;
        const char *func;
        size_t i, t;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in || in->type != JSON_OBJECT) { if (in) json_free(in); continue; }
        func = json_get_str(in, "func", "");
        out = NULL;
        for (t = 0; t < sizeof(W_TABLE) / sizeof(W_TABLE[0]); t++) {
            if (!strcmp(func, W_TABLE[t].name)) { out = W_TABLE[t].fn(in); break; }
        }
        if (!out) out = json_string("UNKNOWN_FUNC");
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
