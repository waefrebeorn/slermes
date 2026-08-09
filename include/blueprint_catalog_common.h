/* blueprint_catalog_common.h — shared accessor to the single baked blueprint
 * catalog source-of-truth (defined in port_blueprint_catalog_helpers.c).
 * Minimal include. */
#ifndef HERMES_BLUEPRINT_CATALOG_COMMON_H
#define HERMES_BLUEPRINT_CATALOG_COMMON_H

#include <json.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Returns the static JSON array string for the curated catalog. Do NOT free. */
const char *blueprint_catalog_raw_json(void);

/* Returns a malloc'd copy of the blueprint object for `key`, or NULL. */
json_t *blueprint_catalog_get(const char *key);
/* Render one blueprint object as a catalog entry JSON string (malloc'd). */
char *blueprint_catalog_entry(json_t *bp);
/* Fill a blueprint's slots from values_json; returns create_job kwargs JSON
 * string (malloc'd) or NULL on validation error (ebuf[ebsz] receives msg). */
char *blueprint_catalog_fill(json_t *bp, const char *values_json,
                             const char *origin_json, char *ebuf, size_t ebsz);

#ifdef __cplusplus
}
#endif
#endif /* HERMES_BLUEPRINT_CATALOG_COMMON_H */
