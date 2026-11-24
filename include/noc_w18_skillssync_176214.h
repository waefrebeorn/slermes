#ifndef NOC_W18_SKILLSSYNC_176214_H
#define NOC_W18_SKILLSSYNC_176214_H

#include "hermes_json.h"

/* C home for upstream Hermes tools/skills_sync_bundled_ops.py.
 * bundled_names: JSON array of bundled skill names (the '_bundled_state()[3]' keys).
 * Returns a freshly allocated name on match (caller frees), NULL otherwise. */
char *w18_skillssync_176214_bundled_skill_for_install(const char *identifier,
                                                      const json_t *bundled_names);

#endif /* NOC_W18_SKILLSSYNC_176214_H */
