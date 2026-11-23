/*
 * nous_wire.c — real C11 port of upstream Hermes agent/nous_wire.py.
 *
 * PoP: nouswire_classify @ agent/nous_wire.py:classify_upstream
 * PoP: nouswire_for_upstream @ agent/nous_wire.py:wire_for_upstream
 * PoP: nouswire_maybe_switch @ agent/nous_wire.py:maybe_switch_wire_after_first_response
 */
#include "noc_w5_nouswire_774ca4.h"

#include <ctype.h>
#include <strings.h>

/* ^gen-\d{9,}-[A-Za-z0-9_-]{8,}$ (ASCII digit/word semantics). */
static int is_openrouter_id(const char *s) {
    const char *p;
    size_t digits, tail;
    if (!s || strncmp(s, "gen-", 4) != 0) return 0;
    p = s + 4;
    digits = 0;
    while (*p && *p >= '0' && *p <= '9') { p++; digits++; }
    if (digits < 9 || *p != '-') return 0;
    p++;
    tail = 0;
    while (*p && (isalnum((unsigned char)*p) || *p == '_' || *p == '-')) { p++; tail++; }
    return tail >= 8 && *p == '\0';
}

json_t *nouswire_classify(const json_t *provider, const json_t *rid) {
    if (provider && provider->type == JSON_STRING && provider->str_val && provider->str_val[0])
        return json_string("openrouter");
    if (rid && rid->type == JSON_STRING && rid->str_val) {
        if (is_openrouter_id(rid->str_val)) return json_string("openrouter");
        if (strncmp(rid->str_val, "msg_", 4) == 0) return json_string("gmi");
    }
    return json_null();
}

json_t *nouswire_for_upstream(const json_t *upstream) {
    /* GMI_NATIVE_WIRE_CLEARED is False upstream, so wire_for_upstream always
     * selects chat_completions regardless of the upstream. */
    (void)upstream;
    return json_string("chat_completions");
}

static json_t *decision(int switched, int decided, json_t *pending) {
    json_t *r = json_object();
    json_set(r, "switched", json_bool(switched ? true : false));
    json_set(r, "decided", json_bool(decided ? true : false));
    json_set(r, "pending", pending ? pending : json_null());
    return r;
}

json_t *nouswire_maybe_switch(const json_t *facts) {
    int call_count, decided;
    const char *provider, *model, *mode, *api_mode;
    json_t *upstream, *pending, *prov_v, *rid_v;

    if (!facts || facts->type != JSON_OBJECT) return decision(0, 0, NULL);
    call_count = (int)json_get_num(facts, "api_call_count", 0);
    decided = json_get_bool(facts, "decided", false) ? 1 : 0;
    if (call_count != 1 || decided) return decision(0, decided, NULL);

    provider = json_get_str(facts, "provider", "");
    if (!provider) provider = "";
    if (strcasecmp(provider, "nous") != 0) return decision(0, decided, NULL);

    model = json_get_str(facts, "model", "");
    if (!model || strncasecmp(model, "anthropic/", 10) != 0) return decision(0, decided, NULL);

    mode = json_get_str(facts, "nous_wire_mode", "");
    if (!mode || strcmp(mode, "auto") != 0) return decision(0, decided, NULL);

    /* The live function sets _nous_wire_decided = True here, whatever the verdict. */
    decided = 1;

    prov_v = json_obj_get(facts, "response_provider");
    rid_v = json_obj_get(facts, "response_id");
    upstream = nouswire_classify(prov_v, rid_v);
    /* want is always "chat_completions" (GMI native wire not cleared). */
    api_mode = json_get_str(facts, "api_mode", "");
    if (api_mode && strcmp(api_mode, "chat_completions") == 0) {
        json_free(upstream);
        return decision(0, decided, NULL);
    }

    pending = json_array();
    json_append(pending, json_string("chat_completions"));
    json_append(pending, upstream); /* string or json null node */
    return decision(1, decided, pending);
}
