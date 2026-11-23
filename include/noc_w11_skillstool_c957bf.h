#ifndef NOC_W11_SKILLSTOOL_C957BF_H
#define NOC_W11_SKILLSTOOL_C957BF_H

#include "hermes_json.h"

/* PoP port of tools/skills_tool_plugin.py (gate w11_skillstool_c957bf). */

/* _json: json.dumps(payload, ensure_ascii=False) with Python's default
 * ", " / ": " separators. Returns a malloc'd string. */
char *w11_skillstool_c957bf_json(json_t *payload);

/* _fail: JSON string {"success": false, "error": <error>, **extra}.
 * Returns a malloc'd string. */
char *w11_skillstool_c957bf_fail(const char *error, json_t *extra);

/* _truncate_description: cap at 1024 code points with a "..." suffix.
 * Returns a malloc'd string. */
char *w11_skillstool_c957bf_truncate_description(const char *description);

#endif /* NOC_W11_SKILLSTOOL_C957BF_H */
