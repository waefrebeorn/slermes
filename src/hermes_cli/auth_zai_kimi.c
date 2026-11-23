/*
 * auth_zai_kimi.c — C home for upstream Hermes hermes_cli/auth_zai_kimi.py.
 *
 * PoP: w10_authzaiki_f897c0_resolve_kimi_base_url @ hermes_cli/auth_zai_kimi.py:_resolve_kimi_base_url
 * PoP: w10_authzaiki_f897c0_normalize_lmstudio_runtime_base_url @ hermes_cli/auth_zai_kimi.py:_normalize_lmstudio_runtime_base_url
 *
 * Faithful C11 port of the two pure URL-resolution helpers. Network probes
 * (_probe_single_zai_endpoint / detect_zai_endpoint / _resolve_zai_base_url)
 * are intentionally out of scope: they issue HTTP and mutate auth state.
 */
#include "noc_w10_authzaiki_f897c0.h"

#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define KIMI_CODE_BASE_URL "https://api.kimi.com/coding"
#define LM_FALLBACK_ROOT   "http://127.0.0.1:1234"

/* Python str(v or "") for a JSON scalar: JSON null / non-string -> "". */
static const char *azk_str(const json_t *v) {
    if (v && v->type == JSON_STRING) return v->str_val ? v->str_val : "";
    return "";
}

static int azk_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    return 1;
}

struct json_t *w10_authzaiki_f897c0_kimi_base(const struct json_t *api_key,
                                              const struct json_t *default_url,
                                              const struct json_t *env_override) {
    if (azk_truthy(env_override)) return json_string(azk_str(env_override));
    const char *key = azk_str(api_key);
    if (key[0] && strncmp(key, "sk-kimi-", 8) == 0)
        return json_string(KIMI_CODE_BASE_URL);
    if (default_url && default_url->type == JSON_STRING)
        return json_string(azk_str(default_url));
    return json_null();
}

struct json_t *w10_authzaiki_f897c0_lmstudio_base(const struct json_t *base_url) {
    char root[4096];
    const char *src = azk_str(base_url);
    size_t n = 0, cap = sizeof(root) - 1;
    /* Python str.strip(): ASCII whitespace both ends. */
    while (*src && isspace((unsigned char)*src)) src++;
    size_t len = strlen(src);
    while (len > 0 && isspace((unsigned char)src[len - 1])) len--;
    for (size_t i = 0; i < len && n < cap; ++i) root[n++] = src[i];
    root[n] = 0;
    /* rstrip("/") */
    while (n > 0 && root[n - 1] == '/') root[--n] = 0;

    static const char *const suffixes[] = { "/api/v1", "/api", "/v1" };
    for (size_t s = 0; s < 3; ++s) {
        size_t sl = strlen(suffixes[s]);
        if (n >= sl && strcmp(root + n - sl, suffixes[s]) == 0) {
            n -= sl;
            while (n > 0 && root[n - 1] == '/') root[--n] = 0;
            root[n] = 0;
            break;
        }
    }
    char out[4200];
    if (n == 0) snprintf(out, sizeof(out), "%s/v1", LM_FALLBACK_ROOT);
    else snprintf(out, sizeof(out), "%s/v1", root);
    return json_string(out);
}
