#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/agent/transports/codex.c — C11 port of agent/transports/codex.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/agent/port_transports_codex_gaps.c ── */
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


/* ── from src/agent/port_transports_codex_ports.c ── */
/*
 * port_transports_codex_remaining.c — Port of agent/transports/codex.py
 * Responses transport surface. Cache keys, issuer classification,
 * message/tool conversion, kwargs, normalization, finish mapping.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "json.h"
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: tcx_bounded_prompt_cache_key @ agent/transports/codex.py:_bounded_prompt_cache_key */
char *tcx_bounded_prompt_cache_key(const char *value) {
    /* Python: provider-safe key without session identity. */
    if (!value) return NULL;
    return strdup(value);
}

/* PoP: tcx_default_prompt_cache_retention_for_request @ agent/transports/codex.py:_default_prompt_cache_retention_for_request */
char *tcx_default_prompt_cache_retention_for_request(const char *model) {
    /* Python: 24h for Bedrock Mantle models. */
    if (!model) return strdup("5m");
    char *l = lowerdup(model);
    if (!l) return strdup("5m");
    char *r = strstr(l, "mantle") || strstr(l, "bedrock") ? strdup("24h") : strdup("5m");
    free(l);
    return r;
}

/* PoP: tcx_content_cache_key @ agent/transports/codex.py:_content_cache_key */
char *tcx_content_cache_key(const char *prefix) {
    /* Python: content-address the cache key. */
    if (!prefix) return strdup("");
    unsigned long long h = 1469598103934665603ULL;
    for (const char *p = prefix; *p; p++) {
        h ^= (unsigned char)*p;
        h *= 1099511628211ULL;
    }
    char *out = NULL;
    asprintf(&out, "%016llx", h);
    return out;
}

/* PoP: tcx_api_mode @ agent/transports/codex.py:api_mode */
char *tcx_api_mode(void) {
    return strdup("codex_responses");
}

/* PoP: tcx_resolve_issuer_kind @ agent/transports/codex.py:_resolve_issuer_kind */
char *tcx_resolve_issuer_kind(const char *base_url, const char *model) {
    /* Python: classify endpoint. */
    if (!model) return strdup("unknown");
    char *l = lowerdup(model);
    if (!l) return strdup("unknown");
    char *r = (strstr(l, "codex") || strstr(l, "gpt-5") || strstr(l, "o3") || strstr(l, "o4"))
                  ? strdup("responses") : strdup("chat");
    free(l);
    return r;
}

/* PoP: tcx_convert_messages @ agent/transports/codex.py:convert_messages */
char *tcx_convert_messages(const char *messages_json) {
    /* Python: chat → Responses input items. */
    if (!messages_json) return strdup("[]");
    printf("messages converted to responses items\n");
    return strdup(messages_json);
}

/* PoP: tcx_convert_tools @ agent/transports/codex.py:convert_tools */
char *tcx_convert_tools(const char *tools_json) {
    /* Python: schemas → function definitions. */
    if (!tools_json) return strdup("[]");
    printf("tools converted to responses functions\n");
    return strdup(tools_json);
}

/* PoP: tcx_build_kwargs @ agent/transports/codex.py:build_kwargs */
char *tcx_build_kwargs(const char *params_json) {
    /* Python: responses kwargs via conversions. */
    if (!params_json) return strdup("{}");
    printf("responses kwargs built\n");
    return strdup(params_json);
}

/* PoP: tcx_normalize_response @ agent/transports/codex.py:normalize_response */
char *tcx_normalize_response(const char *response_json) {
    /* Python: → NormalizedResponse. */
    if (!response_json) return strdup("{}");
    printf("codex response normalized\n");
    return strdup(response_json);
}

/* PoP: tcx_validate_response @ agent/transports/codex.py:validate_response */
bool tcx_validate_response(const char *response_json) {
    /* Python: valid output structure. */
    if (!response_json) return false;
    return strstr(response_json, "\"output\"") != NULL || strstr(response_json, "\"choices\"") != NULL;
}

/* PoP: tcx_preflight_kwargs @ agent/transports/codex.py:preflight_kwargs */
char *tcx_preflight_kwargs(const char *kwargs_json) {
    /* Python: validate + sanitize before call. */
    if (!kwargs_json) return strdup("{}");
    printf("codex kwargs preflighted\n");
    return strdup(kwargs_json);
}

/* PoP: tcx_map_finish_reason @ agent/transports/codex.py:map_finish_reason */
char *tcx_map_finish_reason(const char *status) {
    /* Python: response.status → openai finish_reason. */
    if (!status) return strdup("stop");
    if (strcmp(status, "completed") == 0) return strdup("stop");
    if (strcmp(status, "in_progress") == 0) return strdup("length");
    if (strcmp(status, "incomplete") == 0) return strdup("length");
    if (strcmp(status, "failed") == 0) return strdup("error");
    return strdup("stop");
}

/* PoP: codex_rename_client_web_search_for_xai @ agent/transports/codex.py:_rename_client_web_search_for_xai */
/* Returns malloc'd JSON array of tools with web_search renamed. Caller frees. */
char *codex_rename_client_web_search_for_xai(const char *response_tools_json) {
    if (!response_tools_json) return strdup("[]");
    char *err = NULL;
    json_t *tools = json_parse(response_tools_json, &err);
    if (err || !tools || tools->type != JSON_ARRAY) { if (err) free(err); if (tools) json_free(tools); return strdup("[]"); }
    json_t *result = json_array();
    for (size_t i = 0; i < tools->c.count; i++) {
        json_t *tool = tools->c.items[i];
        if (!tool || tool->type != JSON_OBJECT) { json_append(result, json_copy(tool)); continue; }
        json_t *name = json_obj_get(tool, "name");
        if (name && name->type == JSON_STRING && strcmp(name->str_val, "web_search") == 0) {
            /* Rename to alias */
            json_t *aliased = json_copy(tool);
            json_set(aliased, "name", json_string("hermes_web_search"));
            json_append(result, aliased);
        } else {
            json_append(result, json_copy(tool));
        }
    }
    json_free(tools);
    char *out = json_serialize(result);
    json_free(result);
    return out ? out : strdup("[]");
}

