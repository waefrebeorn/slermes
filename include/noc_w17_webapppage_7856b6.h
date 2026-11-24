#ifndef NOC_W17_WEBAPPPAGE_7856B6_H
#define NOC_W17_WEBAPPPAGE_7856B6_H

#include "hermes_json.h"

/* C home for upstream hermes_cli/webapp_page_policy.py (CSP for the
 * browser-hosted Desktop). Pure HTML script-hash + header policy helpers. */

/* The live module's _ScriptHashes(html).hashes, as JSON array of the
 * quoted hash tokens (e.g. "'sha256-...'"). */
json_t *w17_webapppage_7856b6_script_hashes(const char *html);

/* page_headers with the request.app.state.ui_surface read injected as an
 * explicit seam (NULL means the attribute is absent): webapp gets the CSP,
 * any other surface passes the caller's headers through unchanged. */
json_t *w17_webapppage_7856b6_page_headers_ex(const char *ui_surface,
                                              const char *html,
                                              const json_t *headers);

/* Module-level policy literals, exposed for parity checking. */
json_t *w17_webapppage_7856b6_constants(void);

#endif /* NOC_W17_WEBAPPPAGE_7856B6_H */
