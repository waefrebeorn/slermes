/* Slermes C port — agent/retry_utils.py (pure Z.AI coding-overload classifier) */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Mirrors the error object shape the Python classifier reads:
 * status_code + flattened text (message/body/response lowercased). */
typedef struct {
    int status_code;
    char text[8192];
} retry_utils_err_t;

/* PoP: agent_retry_utils_is_zai_coding_overload_error @ agent/retry_utils.py:is_zai_coding_overload_error */
bool agent_retry_utils_is_zai_coding_overload_error(const char *base_url, const char *model, const retry_utils_err_t *err)
{
    if (!err) return false;
    char base[1024], mdl[1024], txt[8192];
    size_t i, n;
    n = 0; for (const char *p = base_url ? base_url : ""; *p && n + 1 < sizeof(base); p++) base[n++] = (char)tolower((unsigned char)*p); base[n] = '\0';
    n = 0; for (const char *p = model ? model : ""; *p && n + 1 < sizeof(mdl); p++) mdl[n++] = (char)tolower((unsigned char)*p); mdl[n] = '\0';
    n = 0; for (const char *p = err->text; *p && n + 1 < sizeof(txt); p++) txt[n++] = (char)tolower((unsigned char)*p); txt[n] = '\0';

    if (err->status_code != 429) return false;
    if (!strstr(base, "api.z.ai/api/coding/paas/v4")) return false;
    if (!strstr(mdl, "glm-5.2")) return false;
    if (strstr(txt, "1305") || strstr(txt, "temporarily overloaded")) return true;
    return false;
}
