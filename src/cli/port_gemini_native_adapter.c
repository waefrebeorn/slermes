/*
 * port_gemini_native_adapter.c
 *
 * Pure helpers ported from agent/gemini_native_adapter.py (55 defs, previously
 * 0 ported). The httpx / asyncio half stays in Python.
 *
 *   gna_bare_model_id            <- bare_gemini_model_id
 *   gna_version                  <- _gemini_version
 *   gna_requires_tool_call_ids   <- gemini_requires_tool_call_ids
 *   gna_uses_thinking_level      <- gemini_uses_thinking_level
 *   gna_sampling_deprecated      <- gemini_sampling_deprecated
 *   gna_is_vertex_express        <- is_vertex_express_base_url
 *   gna_normalize_base_url       <- normalize_gemini_base_url
 *   gna_is_native_base_url       <- is_native_gemini_base_url
 *   gna_accepts_parameters_schema<- gemini_accepts_parameters_json_schema
 *   gna_is_free_tier_quota_error <- is_free_tier_quota_error
 *   gna_is_standard_key_error    <- is_standard_key_auth_error
 *   gna_coerce_content_to_text   <- _coerce_content_to_text
 *
 * The version helpers are deliberately version-number gates rather than
 * "gemini-3" prefix tests: a new generation (gemini-4-argon) must not be
 * silently treated as legacy.
 */

#include "port_gemini_native_adapter.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "../lib/libjson/json.h"

/* gemini_native_adapter.py:DEFAULT_GEMINI_BASE_URL */
#define GNA_DEFAULT_BASE_URL "https://generativelanguage.googleapis.com/v1beta"

/* ===========================================================================
 *  bare_gemini_model_id  (gemini_native_adapter.py:bare_gemini_model_id)
 *
 *      name = (model or "").strip()
 *      for prefix in ("google/", "gemini/"):
 *          if name.lower().startswith(prefix):
 *              return name[len(prefix):].strip() or name
 *      return name
 *
 *  The `or name` fallback: when stripping the prefix leaves nothing (e.g. the
 *  model id IS "google/"), the ORIGINAL stripped name is returned.
 *  Caller frees.
 * =========================================================================== */

/* PoP: bare_gemini_model_id @ agent/gemini_native_adapter.py:bare_gemini_model_id */
char *gna_bare_model_id(const char *model)
{
    const char *s = (model && model[0]) ? model : "";
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\n' || s[n-1] == '\r')) n--;

    static const char *const prefixes[] = { "google/", "gemini/" };
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++) {
        size_t pl = strlen(prefixes[i]);
        if (n >= pl) {
            int match = 1;
            for (size_t j = 0; j < pl; j++)
                if (tolower((unsigned char)s[j]) != prefixes[i][j]) { match = 0; break; }
            if (match) {
                const char *rest = s + pl;
                size_t rl = n - pl;
                while (rl > 0 && (*rest == ' ' || *rest == '\t')) { rest++; rl--; }
                if (rl == 0) goto fallback;
                char *out = (char *)malloc(rl + 1);
                if (!out) return NULL;
                memcpy(out, rest, rl);
                out[rl] = '\0';
                return out;
            }
        }
    }
fallback: {
        char *out = (char *)malloc(n + 1);
        if (!out) return NULL;
        memcpy(out, s, n);
        out[n] = '\0';
        return out;
    }
}

/* ===========================================================================
 *  _gemini_version  (gemini_native_adapter.py:_gemini_version)
 *
 *      match = re.match(r"gemini-(\d+)(?:\.(\d+))?", bare_gemini_model_id(model).lower())
 *      return (int(match.group(1)), int(match.group(2) or 0)) if match else (0, 0)
 *
 *  `re.match` anchors at the START (not a search). (0, 0) for non-Gemini and
 *  unversioned ids such as "gemini-flash-latest".
 * =========================================================================== */

/* PoP: _gemini_version @ agent/gemini_native_adapter.py:_gemini_version */
void gna_version(const char *model, int *major, int *minor)
{
    char *bare = gna_bare_model_id(model);
    if (!bare) { *major = 0; *minor = 0; return; }
    for (size_t i = 0; bare[i]; i++) bare[i] = (char)tolower((unsigned char)bare[i]);

    *major = 0; *minor = 0;
    if (strncmp(bare, "gemini-", 7) != 0) { free(bare); return; }
    const char *p = bare + 7;   /* "gemini-" is 7 chars */
    if (!isdigit((unsigned char)*p)) { free(bare); return; }
    int maj = 0;
    while (isdigit((unsigned char)*p)) { maj = maj * 10 + (*p - '0'); p++; }
    int min = 0;
    if (*p == '.') {
        p++;
        while (isdigit((unsigned char)*p)) { min = min * 10 + (*p - '0'); p++; }
    }
    *major = maj; *minor = min;
    free(bare);
}

/* ===========================================================================
 *  The four version-gated predicates.
 * =========================================================================== */

/* PoP: gemini_requires_tool_call_ids @ agent/gemini_native_adapter.py:gemini_requires_tool_call_ids */
int gna_requires_tool_call_ids(const char *model)
{
    int maj, min; gna_version(model, &maj, &min);
    return maj >= 3;
}

/* PoP: gemini_uses_thinking_level @ agent/gemini_native_adapter.py:gemini_uses_thinking_level */
int gna_uses_thinking_level(const char *model)
{
    char *bare = gna_bare_model_id(model);
    if (!bare) return 0;
    for (size_t i = 0; bare[i]; i++) bare[i] = (char)tolower((unsigned char)bare[i]);
    size_t n = strlen(bare);
    int is_latest = (strncmp(bare, "gemini-", 7) == 0 && n >= 7 &&
                     strcmp(bare + n - 7, "-latest") == 0);
    free(bare);
    if (is_latest) return 1;
    int maj, min; gna_version(model, &maj, &min);
    return maj >= 3;
}

/* PoP: gemini_sampling_deprecated @ agent/gemini_native_adapter.py:gemini_sampling_deprecated */
int gna_sampling_deprecated(const char *model)
{
    int maj, min; gna_version(model, &maj, &min);
    return maj > 3 || (maj == 3 && min >= 6);
}

/* ===========================================================================
 *  Base-URL classification.
 * =========================================================================== */

/* PoP: is_vertex_express_base_url @ agent/gemini_native_adapter.py:is_vertex_express_base_url */
int gna_is_vertex_express(const char *base_url)
{
    char *n = gna_lower(base_url);
    if (!n) return 0;
    int hit = (strstr(n, "aiplatform.googleapis.com") != NULL &&
               strstr(n, "/endpoints/") == NULL);
    free(n);
    return hit;
}

/* gemini_native_adapter.py:_API_VERSION_SEGMENT = re.compile(r"^v\d+(?:alpha|beta)?\d*$", I) */
static int gna_is_version_segment(const char *seg)
{
    if (!seg || *seg != 'v') return 0;
    seg++;
    if (!isdigit((unsigned char)*seg)) return 0;
    while (isdigit((unsigned char)*seg)) seg++;
    if (strncmp(seg, "alpha", 5) == 0) seg += 5;
    else if (strncmp(seg, "beta", 4) == 0) seg += 4;
    while (isdigit((unsigned char)*seg)) seg++;
    return *seg == '\0';
}

/* PoP: normalize_gemini_base_url @ agent/gemini_native_adapter.py:normalize_gemini_base_url */
char *gna_normalize_base_url(const char *base_url)
{
    /* trimmed = str(base_url or "").strip().rstrip("/") */
    const char *s = (base_url && base_url[0]) ? base_url : "";
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '/' || s[n-1] == ' ' || s[n-1] == '\t')) n--;
    char *trimmed = (char *)malloc(n + 1);
    if (!trimmed) return NULL;
    memcpy(trimmed, s, n);
    trimmed[n] = '\0';

    /* re.sub(r"/openai\Z", "", trimmed, flags=I).rstrip("/") */
    size_t tl = strlen(trimmed);
    if (tl >= 7) {
        const char *tail = trimmed + tl - 7;
        int match = 1;
        const char *pat = "/openai";
        for (int i = 0; i < 7; i++)
            if (tolower((unsigned char)tail[i]) != pat[i]) { match = 0; break; }
        if (match) { trimmed[tl - 7] = '\0'; tl -= 7; }
    }
    while (tl > 0 && trimmed[tl-1] == '/') trimmed[--tl] = '\0';

    if (tl == 0) {
        free(trimmed);
        trimmed = strdup(GNA_DEFAULT_BASE_URL);
        if (!trimmed) return NULL;
        tl = strlen(trimmed);
    }

    if (gna_is_vertex_express(trimmed)) {
        size_t l2 = strlen(trimmed);
        if (l2 >= 18) {                   /* "/publishers/google" is 18 chars */
            const char *tail = trimmed + l2 - 18;
            int match = 1;
            for (int i = 0; i < 18; i++)
                if (tolower((unsigned char)tail[i]) != "/publishers/google"[i]) { match = 0; break; }
            if (match) return trimmed;
        }
        /* the LAST path segment only */
        const char *last = strrchr(trimmed, '/');
        last = last ? last + 1 : trimmed;
        char *lower = gna_lower(trimmed);
        int has_projects = (lower && strstr(lower, "/projects/") != NULL);
        free(lower);
        if (!has_projects && !gna_is_version_segment(last)) {
            size_t cap = tl + 9;              /* 8 chars + NUL */
            char *tmp = (char *)realloc(trimmed, cap);
            if (!tmp) { free(trimmed); return NULL; }
            trimmed = tmp;
            memcpy(trimmed + tl, "/v1beta1", 9);
            tl += 8;
        }
        size_t cap = tl + 19;                 /* 18 chars + NUL */
        char *tmp = (char *)realloc(trimmed, cap);
        if (!tmp) { free(trimmed); return NULL; }
        trimmed = tmp;
        memcpy(trimmed + tl, "/publishers/google", 19);
        return trimmed;
    }

    const char *last = strrchr(trimmed, '/');
    last = last ? last + 1 : trimmed;
    if (gna_is_version_segment(last)) return trimmed;

    size_t cap = tl + 8;                      /* 7 chars + NUL */
    char *tmp = (char *)realloc(trimmed, cap);
    if (!tmp) { free(trimmed); return NULL; }
    trimmed = tmp;
    memcpy(trimmed + tl, "/v1beta", 8);
    return trimmed;
}

/* PoP: is_native_gemini_base_url @ agent/gemini_native_adapter.py:is_native_gemini_base_url */
int gna_is_native_base_url(const char *base_url)
{
    char *n = gna_lower(base_url);
    if (!n) return 0;
    size_t len = strlen(n);
    while (len > 0 && n[len-1] == '/') n[--len] = '\0';
    int result;
    if (len >= 7 && strcmp(n + len - 7, "/openai") == 0) result = 0;
    else result = (strstr(n, "generativelanguage.googleapis.com") != NULL ||
                   gna_is_vertex_express(n));
    free(n);
    return result;
}

/* PoP: gemini_accepts_parameters_json_schema @ agent/gemini_native_adapter.py:gemini_accepts_parameters_json_schema */
int gna_accepts_parameters_schema(const char *base_url)
{
    char *n = gna_lower(base_url);
    if (!n) return 0;
    size_t len = strlen(n);
    while (len > 0 && n[len-1] == '/') n[--len] = '\0';
    int result = (len >= 7 && strcmp(n + len - 7, "/v1beta") == 0);
    free(n);
    return result;
}

/* ===========================================================================
 *  Error classification.
 * =========================================================================== */

/* PoP: is_free_tier_quota_error @ agent/gemini_native_adapter.py:is_free_tier_quota_error */
int gna_is_free_tier_quota_error(const char *error_message)
{
    if (!error_message || !error_message[0]) return 0;
    char *n = gna_lower(error_message);
    if (!n) return 0;
    int hit = strstr(n, "free_tier") != NULL;
    free(n);
    return hit;
}

/* PoP: is_standard_key_auth_error @ agent/gemini_native_adapter.py:is_standard_key_auth_error */
int gna_is_standard_key_error(int status, const char *error_message,
                              const char *reason, const char *api_key)
{
    char *msg = gna_lower(error_message ? error_message : "");
    if (!msg) return 0;
    int result = 0;
    if (status == 401 &&
        ((reason && strcmp(reason, "ACCESS_TOKEN_TYPE_UNSUPPORTED") == 0) ||
         strstr(msg, "expected oauth 2 access token") != NULL)) {
        result = 1;
    } else if (status == 400 &&
               (api_key && strncmp(api_key, "AIza", 4) == 0) &&
               ((reason && strcmp(reason, "API_KEY_INVALID") == 0) ||
                strstr(msg, "api key not valid") != NULL)) {
        result = 1;
    }
    free(msg);
    return result;
}

/* ===========================================================================
 *  _coerce_content_to_text  (gemini_native_adapter.py:_coerce_content_to_text)
 *
 *      if isinstance(content, list):
 *          return "\n".join(t for t in map(_text_of, content) if t is not None)
 *      return "" if content is None else str(content)
 *
 *  `items` is a NULL-terminated array of const char* where a NULL entry is a
 *  non-text item. Caller frees.
 * =========================================================================== */

/* PoP: _coerce_content_to_text @ agent/gemini_native_adapter.py:_coerce_content_to_text */
char *gna_coerce_content_to_text(const char *const *items)
{
    if (!items) return strdup("");
    size_t cap = 1;
    int n = 0;
    for (int i = 0; items[i]; i++) { cap += strlen(items[i]) + 1; n++; }
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';
    for (int i = 0; i < n; i++) {
        if (i) strcat(out, "\n");
        strcat(out, items[i]);
    }
    return out;
}

/* ===========================================================================
 *  Internal helper: lowercased copy.
 * =========================================================================== */

char *gna_lower(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[n] = '\0';
    return out;
}
