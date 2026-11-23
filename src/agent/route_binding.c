/*
 * route_binding.c — C port of upstream Hermes agent/route_binding.py.
 *
 * PoP: w6_routebindin_0a632a_resolve_route_entry_client @ agent/route_binding.py:resolve_route_entry_client
 *
 * Only the pure decision path is ported. The oracle injects deterministic
 * stubs for resolve_provider_client / _fallback_api_mode_hint /
 * resolve_entry_api_key / PoolChoice / get_secret /
 * normalize_model_for_provider / base_url_host_matches; this module reproduces
 * the exact wrapper control flow (base_url/api_key hints, Ollama-Cloud secret
 * fallback, pool-choice creation, model normalization, client gating).
 */
#include "noc_w6_routebindin_0a632a.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python str.strip() (ASCII whitespace) */
static char *strip(const char *s) {
    if (!s) return NULL;
    const char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    const char *e = p + strlen(p);
    while (e > p && isspace((unsigned char)e[-1])) e--;
    char *out = (char *)malloc((size_t)(e - p) + 1);
    memcpy(out, p, (size_t)(e - p)); out[e - p] = '\0';
    return out;
}
static int streq(const char *a, const char *b) { return a && b && !strcmp(a, b); }

/* utils.base_url_host_matches(url, "ollama.com") as injected in the oracle:
 *   rest = url.split("://", 1)[-1]
 *   host = rest.split('/')[0].split('?')[0].split('#')[0].split(':')[0].lower()
 */
static int host_matches_ollama(const char *url) {
    if (!url) return 0;
    const char *p = strstr(url, "://");
    p = p ? p + 3 : url;
    char host[256]; size_t n = 0;
    while (*p && *p != '/' && *p != '?' && *p != '#') {
        if (n + 1 < sizeof(host)) host[n++] = (char)tolower((unsigned char)*p);
        p++;
    }
    /* split at ':' */
    for (size_t i = 0; i < n; i++) if (host[i] == ':') { n = i; break; }
    host[n] = '\0';
    return strcmp(host, "ollama.com") == 0;
}

static char *concat5(const char *a, const char *b, const char *c,
                     const char *d, const char *e) {
    size_t n = strlen(a)+strlen(b)+strlen(c)+strlen(d)+strlen(e)+5; /* 4 '|' + NUL */
    char *out = (char *)malloc(n);
    snprintf(out, n, "%s|%s|%s|%s|%s", a, b, c, d, e);
    return out;
}

json_t *w6_routebindin_0a632a_resolve_route_entry_client(const json_t *entry,
        const char *provider, const char *model, int raw_codex) {
    const char *prov = provider ? provider : "";
    const char *mdl = model ? model : "";

    /* base_url_hint = (entry.get("base_url") or "").strip() or None */
    char *base = strip(json_get_str(entry, "base_url", ""));
    const char *base_hint = (base && *base) ? base : NULL;

    /* api_key_hint = resolve_entry_api_key(entry) -> entry.get("api_key") */
    json_t *ak = entry ? json_obj_get(entry, "api_key") : NULL;
    const char *api_hint = (ak && ak->type == JSON_STRING) ? ak->str_val : NULL;

    /* api_mode_explicit, api_mode = _fallback_api_mode_hint(entry, provider, hint) */
    int mode_explicit = json_get_bool(entry, "api_mode_explicit", false) ? 1 : 0;
    const char *api_mode = json_get_str(entry, "api_mode", "chat_completions");

    /* Ollama Cloud: host match (not substring) + no key -> env secret */
    if (base_hint && host_matches_ollama(base_hint) && (!api_hint || !*api_hint))
        api_hint = "ollama-secret";

    /* PoolChoice(model) unless MoA (choice kept only when a client resolves) */
    int choice = !streq(prov, "moa");

    /* resolve_provider_client stub: None for synthetic provider "no_client" */
    char *client = NULL;
    if (!streq(prov, "no_client"))
        client = concat5(prov, base_hint ? base_hint : "None",
                         api_hint ? api_hint : "None", api_mode,
                         raw_codex ? "True" : "False");

    /* normalize only when a client exists and provider is not moa */
    char *final_model = strdup(mdl);
    if (client && !streq(prov, "moa")) {
        size_t n = strlen(mdl) + 6;
        char *nm = (char *)malloc(n);
        snprintf(nm, n, "norm:%s", mdl);
        free(final_model); final_model = nm;
    }

    json_t *res = json_object();
    json_set(res, "client", client ? json_string(client) : json_null());
    json_set(res, "model", json_string(final_model));
    json_set(res, "api_mode_explicit", json_bool(mode_explicit ? true : false));
    json_set(res, "api_mode", json_string(api_mode ? api_mode : ""));
    json_set(res, "pool_choice", json_bool((choice && client) ? true : false));
    free(client); free(final_model); free(base);
    return res;
}

