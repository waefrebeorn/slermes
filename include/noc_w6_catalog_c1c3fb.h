#ifndef NOC_W6_CATALOG_C1C3FB_H
#define NOC_W6_CATALOG_C1C3FB_H

#include "hermes_json.h"

/* Public C API for the pure catalog projection of acp_adapter/catalog.py
 * (gate w6_catalog_c1c3fb). acpcat_ prefix avoids collisions with the other
 * catalog_* symbols in this repo.
 *
 * Storage is injected: `rows` is the array of list-row dicts read_catalog_rows
 * would return (each with "id"), `owners` maps physical ids to retained logical
 * owners. No filesystem or database is touched. */
struct json_t *acpcat_catalog_sessions(const struct json_t *rows,
                                       const struct json_t *owners);
struct json_t *acpcat_format_updated_at(const struct json_t *value);
struct json_t *acpcat_updated_at_sort_key(const struct json_t *value);

#endif /* NOC_W6_CATALOG_C1C3FB_H */
