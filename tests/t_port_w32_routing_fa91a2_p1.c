/* JSON-lines harness for the w32_routing_fa91a2_p1 gate (routing statements
 * and the coercions that build them).  One request object per stdin line; one
 * response line carrying "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w32_routing_fa91a2_p1.h"

typedef json_t *(*w_fn)(const json_t *);

static const struct { const char *name; w_fn fn; } W_TABLE[] = {
    { "normalize_profile",  w32_routing_fa91a2_normalize_profile },
    { "route_namespace",    w32_routing_fa91a2_route_namespace },
    { "upsert_plan",        w32_routing_fa91a2_upsert_plan },
    { "origin_query",       w32_routing_fa91a2_origin_query },
    { "never_active_query", w32_routing_fa91a2_never_active_query },
    { "peer_plan",          w32_routing_fa91a2_peer_plan },
    { "record_peer_plan",   w32_routing_fa91a2_record_peer_plan },
    { "enable_topic_plan",  w32_routing_fa91a2_enable_topic_plan },
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
