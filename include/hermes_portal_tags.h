#ifndef HERMES_PORTAL_TAGS_H
#define HERMES_PORTAL_TAGS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return the client=... tag for Nous Portal requests.
 *
 * Format: "client=hermes-client-v<MAJOR>.<MINOR>.<PATCH>".
 *
 * @return Newly allocated string. Caller must free().
 */
char *hermes_client_tag(void);

/**
 * Return the canonical list of Nous Portal product tags as a JSON array string.
 *
 * Always returns a fresh JSON string so callers can use it directly
 * in extra_body / tags fields.
 *
 * @return Newly allocated JSON array string. Caller must free().
 */
char *hermes_nous_portal_tags_json(void);

/**
 * Return the Hermes version string (e.g. "0.15.1-slermes").
 * Port of Python: _hermes_version()
 *
 * @return Pointer to static version string (NOT caller-owned).
 */
const char *hermes_version(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_PORTAL_TAGS_H */
