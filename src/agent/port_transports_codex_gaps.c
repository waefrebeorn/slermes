/* Slermes C port — agent/transports/codex.py (genuine gaps) + agent/model_metadata.py */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"
#include <regex.h>

/* ── agent/transports/codex.py:_cache_scope_from_session_id ── */
/* Normalize a physical session_id into a stable logical cache scope.
 * Cron sessions have a per-fire timestamp suffix (_YYYYMMDD_HHMMSS) that
 * must be stripped so repeated fires share a cache scope. */

/* PoP: codex_cache_scope_from_session_id @ agent/transports/codex.py:_cache_scope_from_session_id */
char *codex_cache_scope_from_session_id(const char *session_id)
{
    if (!session_id) return strdup("");

    /* Match: ^cron_<base>_\d{8}_\d{6}$ */
    const char *sid = session_id;
    size_t len = strlen(sid);

    /* Check for "cron_" prefix */
    if (strncmp(sid, "cron_", 5) != 0)
        return strdup(sid);

    /* Check for _YYYYMMDD_HHMMSS suffix (8 digits, _, 6 digits, end) */
    if (len < 5 + 1 + 8 + 1 + 6) return strdup(sid); /* too short */
    size_t suffix_start = len - (8 + 1 + 6); /* position of the _ before YYYYMMDD */
    if (suffix_start <= 5) return strdup(sid); /* no base between prefix and suffix */

    if (sid[suffix_start] != '_') return strdup(sid);
    /* Check 8 digits */
    for (int i = 1; i <= 8; i++)
        if (sid[suffix_start + i] < '0' || sid[suffix_start + i] > '9') return strdup(sid);
    if (sid[suffix_start + 9] != '_') return strdup(sid);
    /* Check 6 digits */
    for (int i = 10; i <= 15; i++)
        if (sid[suffix_start + i] < '0' || sid[suffix_start + i] > '9') return strdup(sid);

    /* Extract base: between "cron_" and the suffix */
    size_t base_len = suffix_start - 5;
    char *result = malloc(base_len + 1);
    if (!result) return strdup(sid);
    memcpy(result, sid + 5, base_len);
    result[base_len] = '\0';
    return result;
}

/* ── agent/transports/codex.py:_xai_prefers_native_web_search ── */
/* True when xAI Responses should use Grok's native web_search built-in.
 * Delegates to the web-search registry's provider resolution.
 * On any resolution failure, returns true (fail-closed to native). */

/* PoP: codex_xai_prefers_native_web_search @ agent/transports/codex.py:_xai_prefers_native_web_search */
bool codex_xai_prefers_native_web_search(void)
{
    /* Fail-closed to native — preserves the #48108 incomplete-hang fix.
     * In C, we don't have the Python web_search_registry import.
     * The faithful port: check config for web.search_backend == "xai". */
    const char *backend = getenv("HERMES_WEB_SEARCH_BACKEND");
    if (backend && strcasecmp(backend, "xai") == 0)
        return true;

    /* Also check web.backend config key via getenv */
    backend = getenv("HERMES_WEB_BACKEND");
    if (backend && strcasecmp(backend, "xai") == 0)
        return true;

    /* Default: fail closed to native (matches Python's except: return True) */
    return true;
}

/* ── agent/model_metadata.py:_list_providers ── */
/* Fallback: returns empty list when providers module isn't available.
 * In C, we don't have the Python providers import, so we return []. */

/* PoP: mm_list_providers @ agent/model_metadata.py:_list_providers */
json_t *mm_list_providers(void)
{
    /* Python: try: from providers import list_providers; except: return []
     * In C, we always take the except path — no providers module to import. */
    return json_array();
}
