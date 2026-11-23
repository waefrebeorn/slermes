#ifndef NOC_W11_SKILLSHUB_A953B1_H
#define NOC_W11_SKILLSHUB_A953B1_H

#include "hermes_json.h"

/* PoP port of tools/skills_hub_clawhub.py (gate w11_skillshub_a953b1). */

/* re.split(r"[^a-z0-9]+", query.lower()), empty terms dropped. Free result. */
json_t *w11_skillshub_a953b1_query_terms(const char *query);

/* ClawHubSource search ranking: sum of the ordered checks plus per-term weights.
 * meta is {"identifier":..., "name":..., "description":...}. */
int w11_skillshub_a953b1_search_score(const char *query, json_t *meta);

/* ClawHubSource._normalize_tags: list -> [str(t)]; dict -> keys except "latest";
 * anything else -> []. Free result. */
json_t *w11_skillshub_a953b1_normalize_tags(json_t *tags);

/* First non-empty stripped owner handle (owner dict -> .handle), else NULL. */
char *w11_skillshub_a953b1_owner_from_payload(json_t *data);

/* Empty expected owner OR no/matching actual owner. */
int w11_skillshub_a953b1_owner_matches(const char *expected_owner, json_t *data);

/* (slug, expected_owner) for slug / clawhub/slug / @owner/slug /
 * clawhub/@owner/slug / owner/skills/slug. NULL result -> unparsable.
 * Returns {"slug": str, "owner": str|null}; caller frees. */
json_t *w11_skillshub_a953b1_parse_identifier(const char *identifier);

/* "clawhub/<slug>" for any ClawHub identifier, NULL otherwise. Free result. */
char *w11_skillshub_a953b1_catalog_identifier(json_t *identifier_value);

/* First non-empty str in a JSON array, else NULL. Free result. */
char *w11_skillshub_a953b1_first_str(json_t *values);

/* {"skill": {...}, "latestVersion", "owner"} flattening. NULL -> json_null. */
json_t *w11_skillshub_a953b1_coerce_skill_payload(json_t *data);

#endif /* NOC_W11_SKILLSHUB_A953B1_H */
