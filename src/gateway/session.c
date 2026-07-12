#include "hermes_logger.h"
#include "hermes_core_types.h"
#include <ctype.h>
#include <string.h>

/*
 * session.c — Port of Python gateway/session.py
 *
 * Name-parity wrapper for gateway session management functions.
 * The heavy session lifecycle lives in server.c (gw_session_create,
 * gw_session_lookup, gw_session_destroy, gw_session_list).
 * This file implements the pure utility functions from session.py
 * that are called by the C code directly.
 *
 * PoP annotations referencing this module: 214
 */

/* Port of Python gateway/session.py:_is_path_unsafe
 *
 * Return true if `value` could traverse outside the sessions dir.
 * Rejects: parent traversal (".."), path separator anywhere ("/" or "\\"),
 * and a leading Windows drive letter ("C:").
 * Legitimate session keys are colon-delimited multi-segment ids
 * ("agent:main:<platform>:...") and never contain these.
 */
int* cli_gateway_session__is_path_unsafe(void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p2; (void)p3; (void)p4; (void)p5;
    if (!p1) {
        int *result = (int*)malloc(sizeof(int));
        if (result) *result = 0; /* false */
        return result;
    }
    const char *s = (const char*)p1;
    /* Check for ".." parent traversal */
    if (strstr(s, "..") != NULL) {
        int *result = (int*)malloc(sizeof(int));
        if (result) *result = 1; /* true */
        return result;
    }
    /* Check for path separators */
    if (strchr(s, '/') != NULL || strchr(s, '\\') != NULL) {
        int *result = (int*)malloc(sizeof(int));
        if (result) *result = 1; /* true */
        return result;
    }
    /* Check for leading Windows drive letter pattern "X:" */
    size_t len = strlen(s);
    if (len >= 2 && isalpha((unsigned char)s[0]) && s[1] == ':') {
        int *result = (int*)malloc(sizeof(int));
        if (result) *result = 1; /* true */
        return result;
    }
    int *result = (int*)malloc(sizeof(int));
    if (result) *result = 0; /* false */
    return result;
}

/* Port of Python gateway/session.py:_session_key_namespace
 *
 * Extract the namespace prefix from a session key.
 * Session keys have the form "agent:main:<platform>:<chat_id>" or
 * "agent:cron:<job_id>". Returns the second segment ("main", "cron", etc.)
 * or NULL on input that doesn't match the expected pattern.
 */
void* cli_gateway_session__session_key_namespace(void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p2; (void)p3; (void)p4; (void)p5;
    if (!p1) return NULL;

    const char *key = (const char*)p1;
    /* Skip first segment (e.g. "agent:") */
    const char *first_colon = strchr(key, ':');
    if (!first_colon) return NULL;

    const char *ns_start = first_colon + 1;
    if (*ns_start == '\0') return NULL;

    /* Find end of namespace segment */
    const char *second_colon = strchr(ns_start, ':');
    size_t ns_len;
    if (second_colon) {
        ns_len = (size_t)(second_colon - ns_start);
    } else {
        ns_len = strlen(ns_start);
    }

    if (ns_len == 0) return NULL;

    char *result = (char*)malloc(ns_len + 1);
    if (!result) return NULL;
    memcpy(result, ns_start, ns_len);
    result[ns_len] = '\0';
    return result;
}

/* Port of Python gateway/session.py:_resolve_profile_for_key
 *
 * Extract the profile name from a session key.
 * Session keys have the form "agent:main:<platform>:<chat_id>".
 * The profile is the third segment (after agent: and namespace:).
 * Returns malloc'd string or NULL.
 */
void* cli_gateway_session__resolve_profile_for_key(void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p2; (void)p3; (void)p4; (void)p5;
    if (!p1) return NULL;

    const char *key = (const char*)p1;
    /* Skip first segment "agent:" */
    const char *first_colon = strchr(key, ':');
    if (!first_colon) return NULL;

    /* Skip namespace segment */
    const char *ns_end = strchr(first_colon + 1, ':');
    if (!ns_end) return NULL;

    const char *profile_start = ns_end + 1;
    if (*profile_start == '\0') return NULL;

    /* Profile extends to next colon or end of string */
    const char *third_colon = strchr(profile_start, ':');
    size_t profile_len;
    if (third_colon) {
        profile_len = (size_t)(third_colon - profile_start);
    } else {
        profile_len = strlen(profile_start);
    }

    if (profile_len == 0) return NULL;

    char *result = (char*)malloc(profile_len + 1);
    if (!result) return NULL;
    memcpy(result, profile_start, profile_len);
    result[profile_len] = '\0';
    return result;
}