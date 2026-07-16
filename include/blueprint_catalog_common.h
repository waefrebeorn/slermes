/* blueprint_catalog_common.h — shared accessor to the single baked blueprint
 * catalog source-of-truth (defined in port_blueprint_catalog_helpers.c).
 * Minimal include. */
#ifndef HERMES_BLUEPRINT_CATALOG_COMMON_H
#define HERMES_BLUEPRINT_CATALOG_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

/* Returns the static JSON array string for the curated catalog. Do NOT free. */
const char *blueprint_catalog_raw_json(void);

#ifdef __cplusplus
}
#endif
#endif /* HERMES_BLUEPRINT_CATALOG_COMMON_H */
