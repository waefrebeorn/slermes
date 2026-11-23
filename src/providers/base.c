/*
 * base.c — C home for upstream Hermes providers/base.py
 * Gate: w5_base_c4cc9e
 *
 * PoP: w5_base_c4cc9e_get_hostname @ providers/base.py:ProviderProfile.get_hostname
 * PoP: w5_base_c4cc9e_get_max_tokens @ providers/base.py:ProviderProfile.get_max_tokens
 * PoP: w5_base_c4cc9e_api_kwargs @ providers/base.py:ProviderProfile.build_api_kwargs_extras
 * PoP: w5_base_c4cc9e_prepare_messages @ providers/base.py:ProviderProfile.prepare_messages
 * PoP: w5_base_c4cc9e_supported_reasoning_efforts @ providers/base.py:ProviderProfile.supported_reasoning_efforts
 * PoP: w5_base_c4cc9e_default_vision_model @ providers/base.py:ProviderProfile.default_vision_model
 * PoP: w5_base_c4cc9e_resolve_aux_model @ providers/base.py:ProviderProfile.resolve_aux_model
 * PoP: w5_base_c4cc9e_build_extra_body @ providers/base.py:ProviderProfile.build_extra_body
 * PoP: w5_base_c4cc9e_default_reasoning_config @ providers/base.py:ProviderProfile.default_reasoning_config
 *
 * ProviderProfile is declarative; these are its pure default hooks. The stateful
 * ones (client construction, streaming, credential rotation) stay on AIAgent.
 */
#include "noc_w5_base_c4cc9e.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* urllib.parse.urlparse(base_url).hostname, lowercased, brackets removed. */
static char *base_url_hostname(const char *raw) {
    const char *s = raw ? raw : "";
    const char *net = NULL;
    const char *scheme = strstr(s, "://");
    if (scheme) net = scheme + 3;
    else if (s[0] == '/' && s[1] == '/') net = s + 2;
    else return strdup("");
    const char *end = net;
    while (*end && *end != '/' && *end != '?' && *end != '#') end++;
    const char *host = net;
    for (const char *p = net; p < end; p++) if (*p == '@') host = p + 1;
    const char *hend = end;
    if (*host == '[') {
        const char *rb = memchr(host, ']', (size_t)(end - host));
        if (rb) { host++; hend = rb; }
    } else {
        const char *colon = memchr(host, ':', (size_t)(end - host));
        if (colon) hend = colon;
    }
    size_t n = (size_t)(hend - host);
    char *out = (char *)malloc(n + 1);
    if (!out) return strdup("");
    memcpy(out, host, n); out[n] = '\0';
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)out[i]);
    return out;
}

char *w5_base_c4cc9e_get_hostname(const char *hostname, const char *base_url) {
    const char *h = hostname ? hostname : "";
    if (*h) return strdup(h);                 /* explicit hostname wins verbatim */
    const char *b = base_url ? base_url : "";
    if (!*b) return strdup("");
    return base_url_hostname(b);
}

struct json_t *w5_base_c4cc9e_get_max_tokens(int is_null, const char *decimal) {
    if (is_null) return json_null();
    return json_number((double)strtoll(decimal ? decimal : "0", NULL, 10));
}

struct json_t *w5_base_c4cc9e_api_kwargs(void) {
    struct json_t *r = json_object();
    json_set(r, "extra_body", json_object());
    json_set(r, "top_level", json_object());
    return r;
}

struct json_t *w5_base_c4cc9e_prepare_messages(const struct json_t *messages) {
    return messages ? json_copy(messages) : json_null();
}

struct json_t *w5_base_c4cc9e_supported_reasoning_efforts(void) { return json_null(); }
struct json_t *w5_base_c4cc9e_default_vision_model(void) { return json_null(); }
struct json_t *w5_base_c4cc9e_default_reasoning_config(void) { return json_null(); }
struct json_t *w5_base_c4cc9e_resolve_aux_model(void) { return json_string(""); }
struct json_t *w5_base_c4cc9e_build_extra_body(void) { return json_object(); }
