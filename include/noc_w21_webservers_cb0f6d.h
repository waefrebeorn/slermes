#ifndef NOC_W21_WEBSERVERS_CB0F6D_H
#define NOC_W21_WEBSERVERS_CB0F6D_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/web_server_static_assets.py (gate w21_webservers_cb0f6d).
 *
 * Which request paths are files rather than client-side routes, and where the
 * brand fonts are read from when the served dist has no `fonts/` directory.
 */

/* PoP: w21_webservers_cb0f6d_is_static_asset_path @ hermes_cli/web_server_static_assets.py:is_static_asset_path
 * `full_path in STATIC_ASSET_FILES or full_path.startswith(STATIC_ASSET_PREFIXES)`
 * with STATIC_ASSET_FILES = {apple-touch-icon.png, favicon.ico} and the prefixes
 * assets/, ds-assets/, fonts/, fonts-terminal/. NULL reads as "" (a miss). */
int w21_webservers_cb0f6d_is_static_asset_path(const char *full_path);

/* PoP: w21_webservers_cb0f6d_public_font_file @ hermes_cli/web_server_static_assets.py:public_font_file
 * `public_font_file(dist, dashboard_dist, full_path)` over three roots, in that
 * order: the served dist, the packaged dashboard dist, then the source
 * checkout's `web/public` (the config seam `_WEB_PUBLIC`, passed in by the
 * caller). A hit must be a regular file whose resolved path stays inside the
 * resolved `<root>/<folder>` (`is_relative_to`); the search stops at the first
 * hit. The three roots are absolute directories; the result is the absolute
 * candidate path with '/' separators, or NULL. Caller frees. */
char *w21_webservers_cb0f6d_public_font_file(const char *dist_root, const char *dashboard_root,
                                            const char *web_public_root, const char *full_path);

/* The static-asset vocabularies the module declares, as JSON:
 * {"prefixes":[...], "files":[...], "public_font_dirs":[...]} (declaration order). */
json_t *w21_webservers_cb0f6d_vocabulary(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_WEBSERVERS_CB0F6D_H */
