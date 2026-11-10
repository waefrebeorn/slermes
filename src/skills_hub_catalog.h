/*
 * skills_hub_catalog.h — skills-hub catalog helpers.
 *
 * Module-internal header (skills_hub.c <-> skills_hub_catalog.c).
 */
#ifndef SKILLS_HUB_CATALOG_H
#define SKILLS_HUB_CATALOG_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_skills_hub.h"   /* skill_source_t, hub_skill_meta_t, skills_catalog_t */

bool ensure_dir(const char *dir);
bool fetch_browsesh_source(skill_source_t *src);
int search_catalog(const skills_catalog_t *cat, const char *query, hub_skill_meta_t *results, int limit, int offset);

#endif /* SKILLS_HUB_CATALOG_H */
