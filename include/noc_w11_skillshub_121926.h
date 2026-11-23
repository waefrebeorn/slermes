#ifndef NOC_W11_SKILLSHUB_121926_H
#define NOC_W11_SKILLSHUB_121926_H

#include "hermes_json.h"

/* PoP port of tools/skills_hub_sources.py (gate w11_skillshub_121926). */

/* WellKnownSkillSource._query_to_index_url: normalized index.json URL, or NULL. */
char *w11_skillshub_121926_query_to_index_url(const char *query);

/* WellKnownSkillSource._wrap_identifier: well-known:<base.rstrip('/')>/<name>. */
char *w11_skillshub_121926_wrap_identifier(const char *base_url, const char *skill_name);

/* WellKnownSkillSource._parse_identifier: dict{index_url,base_url,skill_name,
 * skill_url} or json_null. Caller frees. */
json_t *w11_skillshub_121926_parse_wellknown_identifier(const char *identifier);

/* UrlSource._is_valid_skill_name: ^[a-z][a-z0-9_-]*$ after strip().lower(),
 * excluding the reserved names. */
int w11_skillshub_121926_is_valid_skill_name(json_t *name);

/* UrlSource._resolve_skill_name: frontmatter name or URL slug, NULL if none. */
char *w11_skillshub_121926_resolve_skill_name(json_t *fm, const char *url);

/* UrlSource._matches: bare HTTP(S) URL ending in .md (not well-known/index). */
int w11_skillshub_121926_url_matches(json_t *identifier);

/* LobeHubSource._agent_id: strip the "lobehub/" prefix. */
char *w11_skillshub_121926_agent_id(const char *identifier);

/* BrowseShSource._slug_from_identifier: strip the "browse-sh/" prefix. */
char *w11_skillshub_121926_slug_from_identifier(const char *identifier);

#endif /* NOC_W11_SKILLSHUB_121926_H */
