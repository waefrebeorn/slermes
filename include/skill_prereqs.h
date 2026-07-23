#ifndef HERMES_SKILL_PREREQS_H
#define HERMES_SKILL_PREREQS_H

/*
 * skill_prereqs.h — C11 port of pure helpers from tools/skills_tool.py.
 *
 * Models frontmatter as a libjson object.
 */

#include "json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of _normalize_prerequisite_values: filter truthy-stripped string items. */
json_t *skill_prereqs_normalize_values(const json_t *value);

/* Port of _collect_prerequisite_values: (env_vars, commands) from frontmatter. */
void skill_prereqs_collect_values(const json_t *frontmatter,
                                   json_t **out_env_vars, json_t **out_commands);

/* Port of _normalize_setup_metadata: {help, collect_secrets}. */
json_t *skill_prereqs_normalize_setup(const json_t *frontmatter);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_SKILL_PREREQS_H */
