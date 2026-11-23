#ifndef NOC_W11_SKILLSHUB_E6CF0A_H
#define NOC_W11_SKILLSHUB_E6CF0A_H

#include "hermes_json.h"

/* PoP port of tools/skills_hub_install.py (gate w11_skillshub_e6cf0a). */

/* bundle_content_hash: SHA-256 over sorted (path normalized '\' -> '/', NUL,
 * content) pairs, returned as "sha256:" + first 16 hex digits. Caller frees.
 * `files` is {relative_path: text_content}. */
char *w11_skillshub_e6cf0a_bundle_content_hash(json_t *files);

/* _source_matches: source.source_id() == {"skills.sh": "skills-sh"}.get(name, name). */
int w11_skillshub_e6cf0a_source_matches(const char *source_id, const char *source_name);

#endif /* NOC_W11_SKILLSHUB_E6CF0A_H */
