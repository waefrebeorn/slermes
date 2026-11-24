#ifndef NOC_W22_WEBASSETMI_0A1F41_H
#define NOC_W22_WEBASSETMI_0A1F41_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/web_asset_mime_types.py (gate w22_webassetmi_0a1f41).
 *
 * The module is a pure pin table plus the idempotent `mimetypes.add_type` loop
 * that overrides a host (Windows registry) map. The C carries the table and the
 * suffix decisions; the actual mutation of the process-global `mimetypes` map
 * is a live-Python side effect and stays upstream.
 */

/* PoP: w22_webassetmi_0a1f41_pairs @ hermes_cli/web_asset_mime_types.py:WEB_ASSET_MIME_TYPES
 * [["<ext>", "<mime>"], ...] in the dict's insertion order. */
json_t *w22_webassetmi_0a1f41_pairs(void);

/* PoP: w22_webassetmi_0a1f41_mime_for @ hermes_cli/web_asset_mime_types.py:WEB_ASSET_MIME_TYPES
 * WEB_ASSET_MIME_TYPES[ext]: exact, case-sensitive dict lookup. NULL when the
 * extension is not pinned. */
const char *w22_webassetmi_0a1f41_mime_for(const char *ext);

/* PoP: w22_webassetmi_0a1f41_pinned_map @ hermes_cli/web_asset_mime_types.py:normalize_web_asset_mime_types
 * The map `mimetypes.add_type(mime, ext, strict=True)` leaves behind:
 * {ext: types_map[ext]} for every pinned extension, insertion order. */
json_t *w22_webassetmi_0a1f41_pinned_map(void);

/* PoP: w22_webassetmi_0a1f41_guess @ hermes_cli/web_asset_mime_types.py:normalize_web_asset_mime_types
 * The Content-Type `mimetypes.guess_type(url)[0]` answers once the pins are
 * applied, for a URL/path whose suffix is pinned: the lowercased suffix of the
 * last path segment. NULL for no suffix, a dotfile, or an unpinned suffix. */
const char *w22_webassetmi_0a1f41_guess(const char *url);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W22_WEBASSETMI_0A1F41_H */
