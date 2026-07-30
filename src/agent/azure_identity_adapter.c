/*
 * azure_identity_adapter.c — Azure Entra ID token provider adapter.
 *
 * Port of Python agent/azure_identity_adapter.py (555 lines).
 * ALL 12 functions + EntraIdentityConfig class are N/A — they wrap the
 * Python azure-identity SDK (DefaultAzureCredential, get_bearer_token_provider,
 * get_token) which has no C equivalent. The C provider_azure.c handles Azure
 * API-key and direct-token auth natively. Entra ID bearer-token auth in C
 * would require a direct MSAL/libcurl implementation against Azure AD —
 * not yet implemented.
 *
 * N/A: has_azure_identity_installed() — Python import check
 * N/A: _require_azure_identity() — Python lazy install
 * N/A: reset_credential_cache() — Python lru_cache clear
 * N/A: EntraIdentityConfig dataclass — Python frozen dataclass
 *      (EXCEPT __post_init__ — ported below as pure logic)
 * N/A: _build_default_credential() — Azure SDK DefaultAzureCredential constructor
 * N/A: build_credential() — Azure SDK + lru_cache wrapper
 * N/A: build_token_provider() — Azure SDK get_bearer_token_provider
 * N/A: has_azure_identity_credentials() — Azure SDK get_token + threading
 * N/A: describe_active_credential() — Azure SDK + env diagnostic
 * N/A: is_token_provider() — Python callable detection
 * N/A: materialize_bearer_for_http() — Azure SDK token callable invocation
 * N/A: build_bearer_http_client() — Python httpx.Client wrapper
 * Port of Python: has_azure_identity_installed — N/A, Python import check
 * Port of Python: _require_azure_identity — N/A, Python lazy install
 * Port of Python: reset_credential_cache — N/A, Python lru_cache
 * Port of Python: _build_default_credential — N/A, Azure SDK constructor
 * Port of Python: build_credential — N/A, Azure SDK + lru_cache wrapper
 * Port of Python: build_token_provider — N/A, Azure SDK get_bearer_token_provider
 * Port of Python: has_azure_identity_credentials — N/A, Azure SDK
 * Port of Python: describe_active_credential — N/A, Azure SDK + env diagnostic
 * Port of Python: is_token_provider — N/A, Python callable detection
 * Port of Python: materialize_bearer_for_http — N/A, Azure SDK token callable
 * Port of Python: build_bearer_http_client — N/A, Python httpx.Client wrapper
 */

#include "hermes_core_types.h"
#include <string.h>
#include <ctype.h>

/* PoP: agent_azure_identity_adapter_entra_identity_config_post_init @ agent/azure_identity_adapter.py:EntraIdentityConfig.__post_init__ */
/* Frozen-dataclass __post_init__: normalize scope — strip() it, falling back to
 * the documented Foundry default when empty/None. The C port takes the raw
 * scope string and writes the normalized form into out[] (caller-allocated). */
void agent_azure_identity_adapter_entra_identity_config_post_init(
    const char *scope, char *out, size_t outsz)
{
    static const char *DEFAULT = "https://ai.azure.com/.default";
    if (!out || outsz == 0) return;

    /* str(scope or "") */
    const char *s = scope ? scope : "";
    /* strip() leading/trailing whitespace */
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t L = strlen(s);
    while (L > 0 && (s[L-1]==' '||s[L-1]=='\t'||s[L-1]=='\n'||s[L-1]=='\r')) L--;
    if (L == 0) {
        snprintf(out, outsz, "%s", DEFAULT);
        return;
    }
    if (L >= outsz) L = outsz - 1;
    memcpy(out, s, L);
    out[L] = '\0';
}