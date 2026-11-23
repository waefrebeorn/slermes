/*
 * auxiliary_local_runtime.c — C port of upstream Hermes agent/auxiliary_local_runtime.py.
 * PoP: auxlocal_bare_llamacpp_endpoint @ agent/auxiliary_local_runtime.py:bare_llamacpp_endpoint
 *
 * The alias guard, truthiness chain and tuple construction are faithful. Live
 * server discovery is a resolved-fact seam (documented in the header): the
 * literal LLAMACPP_ALIASES membership and the returned (base_url, api_key)
 * expression are real; no managed endpoint is invented.
 */
#include "noc_auxlocal.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *dup_str(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}

/* Python str.strip() over ASCII whitespace + str.lower(), into a new buffer. */
static char *py_strip_lower(const char *s) {
    if (!s) return dup_str("");
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    char *p = (char *)malloc(b - a + 1);
    if (p) {
        for (size_t i = a; i < b; i++)
            p[i - a] = (char)tolower((unsigned char)s[i]);
        p[b - a] = '\0';
    }
    return p;
}

/* LLAMACPP_ALIASES = frozenset({"llamacpp", "llama.cpp", "llama-cpp"}) */
static int is_llamacpp_alias(const char *alias) {
    static const char *const ALIASES[] = {"llamacpp", "llama.cpp", "llama-cpp"};
    for (size_t i = 0; i < sizeof(ALIASES) / sizeof(ALIASES[0]); i++)
        if (strcmp(alias, ALIASES[i]) == 0) return 1;
    return 0;
}

static int truthy_str(const char *s) { return s && s[0] != '\0'; }

json_t *auxlocal_bare_llamacpp_endpoint(const char *provider,
        const char *base_url_in, const char *api_key,
        int named_custom_exists,
        int endpoint_present, const char *ep_base_url, const char *ep_api_key) {
    /* Not a bare alias: explicit base_url short-circuits. */
    if (truthy_str(base_url_in)) return json_null();

    char *alias = py_strip_lower(provider ? provider : "");
    int ok = is_llamacpp_alias(alias);
    free(alias);
    if (!ok || named_custom_exists) return json_null();

    /* endpoint = resolve(...) or {} ; str(endpoint.get("base_url") or "").strip() */
    const char *raw_base = (endpoint_present && ep_base_url) ? ep_base_url : "";
    char *base = dup_str(raw_base);
    if (!base) return json_null();
    size_t n = strlen(base), a = 0, b = n;
    while (a < b && isspace((unsigned char)base[a])) a++;
    while (b > a && isspace((unsigned char)base[b - 1])) b--;
    char *trimmed = (char *)malloc(b - a + 1);
    if (trimmed) {
        memcpy(trimmed, base + a, b - a);
        trimmed[b - a] = '\0';
    }
    free(base);
    if (!trimmed) return json_null();

    /* api_key or endpoint.get("api_key") or "no-key-required" */
    const char *chosen;
    if (truthy_str(api_key)) chosen = api_key;
    else if (endpoint_present && truthy_str(ep_api_key)) chosen = ep_api_key;
    else chosen = "no-key-required";

    json_t *out = json_array();
    json_append(out, json_string(trimmed));
    json_append(out, json_string(chosen));
    free(trimmed);
    return out;
}
