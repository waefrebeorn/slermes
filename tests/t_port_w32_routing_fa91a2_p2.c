/* JSON-lines harness for the w32_routing_fa91a2_p2 gate (routing index reads,
 * orphan/adoption decisions, topic-binding guard).  One request object per
 * stdin line; one response line carrying "out" plus every input key echoed
 * back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w32_routing_fa91a2_p2.h"

typedef json_t *(*w_fn)(const json_t *);

static const struct { const char *name; w_fn fn; } W_TABLE[] = {
    { "routing_entries_for_sessions", w32_routing_fa91a2_routing_entries_for_sessions },
    { "routing_entry_for_session",    w32_routing_fa91a2_routing_entry_for_session },
    { "session_for_origin_pick",      w32_routing_fa91a2_session_for_origin_pick },
    { "orphaned_sessions",            w32_routing_fa91a2_orphaned_sessions },
    { "adopt_gate",                   w32_routing_fa91a2_adopt_gate },
    { "bind_guard",                   w32_routing_fa91a2_bind_guard },
    { "replace_entries",              w32_routing_fa91a2_replace_entries },
    { "prune_outcome",                w32_routing_fa91a2_prune_outcome },
};

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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
