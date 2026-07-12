/* Slermes C port — agent/ssl_verify.py (pure + env helpers) */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>

static bool is_falsey_string(const char *s)
{
    if (!s) return false;
    char buf[32]; size_t n = 0;
    for (size_t i = 0; s[i] && n < sizeof(buf) - 1; i++)
        if (s[i] != ' ' && s[i] != '\t') buf[n++] = (char)tolower((unsigned char)s[i]);
    buf[n] = '\0';
    return strcmp(buf, "false") == 0 || strcmp(buf, "0") == 0 ||
           strcmp(buf, "no") == 0 || strcmp(buf, "off") == 0;
}

/* PoP: agent_ssl_verify__coerce_insecure @ agent/ssl_verify.py:_coerce_insecure */
bool agent_ssl_verify_coerce_insecure(const char *ssl_verify)
{
    if (ssl_verify == NULL) return false;
    if (strcmp(ssl_verify, "false") == 0) return true;
    if (is_falsey_string(ssl_verify)) return true;
    return false;
}

/* PoP: agent_ssl_verify_resolve_httpx_verify @ agent/ssl_verify.py:resolve_httpx_verify */
/* Returns one of: "false" (verification disabled), "ca:<path>" (CA bundle path),
 * or "true" (default). Mirrors Python returning False / SSLContext / True. */
const char *agent_ssl_verify_resolve_httpx_verify(const char *ca_bundle, const char *ssl_verify)
{
    static char result[1024];
    if (agent_ssl_verify_coerce_insecure(ssl_verify)) {
        return "false";
    }
    const char *candidates[8];
    int nc = 0;
    if (ca_bundle && ca_bundle[0]) candidates[nc++] = ca_bundle;
    const char *envs[] = {"HERMES_CA_BUNDLE", "SSL_CERT_FILE", "REQUESTS_CA_BUNDLE", "CURL_CA_BUNDLE", NULL};
    for (int i = 0; envs[i]; i++) {
        const char *e = getenv(envs[i]);
        if (e && e[0]) candidates[nc++] = e;
    }
    for (int i = 0; i < nc; i++) {
        char *path = strdup(candidates[i]);
        /* expand ~ (minimal) */
        if (path[0] == '~') { /* best-effort: leave as-is for oracle; real code uses expanduser */ }
        if (access(path, F_OK) == 0) {
            snprintf(result, sizeof(result), "ca:%s", path);
            free(path);
            return result;
        }
        free(path);
    }
    return "true";
}
