#ifndef NOC_W22_DESTINATIO_E2C9BB_H
#define NOC_W22_DESTINATIO_E2C9BB_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/config_backends/remote/destination.py
 * (gate w22_destinatio_e2c9bb).
 *
 * The plane URL and the IdP token URL are operator-selected, so where a
 * credential may go is a transport policy, not a host allow-list: https only
 * (plain http just for loopback), one unambiguous origin (no user info, query
 * or fragment), and no redirect hop ever re-authorized. The three pure pieces
 * of that policy are ported here; `open_credentialed` (which builds a urllib
 * opener that refuses every 3xx) is a live network seam and stays upstream.
 *
 * urlsplit's shape is reproduced rather than approximated -- the same
 * component split, the same `Invalid IPv6 URL` / `Port could not be cast ...`
 * ValueErrors -- because the error text is user-facing copy.
 */

/* PoP: w22_destinatio_e2c9bb_is_loopback @ plugins/config_backends/remote/destination.py:_is_loopback
 * 1 for the literal host "localhost" or any loopback IP literal (127.0.0.0/8,
 * ::1, and ::ffff:127.0.0.1); 0 for everything else, including a name that only
 * looks local ("LOCALHOST", "127.1") -- `ipaddress.ip_address` rejects those. */
int w22_destinatio_e2c9bb_is_loopback(const char *host);

/* PoP: w22_destinatio_e2c9bb_display_url @ plugins/config_backends/remote/destination.py:display_url
 * `url` for messages: any user info dropped so a misconfigured URL never
 * echoes a credential; an unparsable one is cut at the last '@' instead. */
json_t *w22_destinatio_e2c9bb_display_url(const char *url);

/* PoP: w22_destinatio_e2c9bb_checked_url @ plugins/config_backends/remote/destination.py:checked_url
 * {"ok": true, "url": "<url as given>"} when a credential may be sent there,
 * else {"ok": false, "error": "<CredentialDestinationError message>"}. */
json_t *w22_destinatio_e2c9bb_checked_url(const char *url, const char *env_name);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W22_DESTINATIO_E2C9BB_H */
