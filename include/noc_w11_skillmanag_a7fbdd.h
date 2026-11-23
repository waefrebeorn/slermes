#ifndef NOC_W11_SKILLMANAG_A7FBDD_H
#define NOC_W11_SKILLMANAG_A7FBDD_H

#include "hermes_json.h"

/* PoP port of tools/skill_manager_scan.py (gate w11_skillmanag_a7fbdd). */

/* _finding_keys(skill_dir, findings) with file contents injected as `files`:
 * {filename: text}. Returns a JSON array of
 * [file, pattern_id, severity, sha256(source-line-or-fallback)].
 * Caller frees the result. The scanner reads the file bytes in Python; the C
 * side receives those bytes directly so it never touches the filesystem. */
json_t *w11_skillmanag_a7fbdd_finding_keys(json_t *files, json_t *findings);

#endif /* NOC_W11_SKILLMANAG_A7FBDD_H */
