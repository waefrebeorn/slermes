#ifndef NOC_W12_SKILLSHUB_B005D3_H
#define NOC_W12_SKILLSHUB_B005D3_H

#include "hermes_json.h"

/* PoP port of tools/skills_hub_official.py (gate w12_skillshub_b005d3). */

json_t *w12_skillshub_b005d3_strip_prefix(json_t *value, json_t *prefixes);
json_t *w12_skillshub_b005d3_clean_rel_parts(json_t *path);   /* array | null */
json_t *w12_skillshub_b005d3_entry_provider(json_t *entry);    /* string */
json_t *w12_skillshub_b005d3_optional_rel(json_t *identifier); /* string */
json_t *w12_skillshub_b005d3_upstream_pointer(json_t *fm);     /* object | null */
json_t *w12_skillshub_b005d3_to_meta(json_t *entry);           /* object */

#endif /* NOC_W12_SKILLSHUB_B005D3_H */
