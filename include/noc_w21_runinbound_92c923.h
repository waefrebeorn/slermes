#ifndef NOC_W21_RUNINBOUND_92C923_H
#define NOC_W21_RUNINBOUND_92C923_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of gateway/run_inbound_unauthorized.py (gate w21_runinbound_92c923).
 *
 * Copy and owner-side signalling for unauthorized inbound senders: the pairing
 * reply (code, validity, approve command), the rate-limited reply, the owner's
 * one-line hint (display name neutralized, mention/link sigils stripped, the
 * allowlist env var for the platform), and the per-(platform,user) first-drop
 * memory with its oldest-first eviction.
 *
 * Not ported (bound to a live gateway): `UnauthorizedOwnerNotifier.notify`
 * (home-channel transports, `present_notification`, `runner._send_home_channel_message`)
 * and the plugin registry behind `_allowlist_env_for_platform` -- the table here
 * is that registry's `allowed_users_env` declaration for the bundled platforms.
 */

/* PoP: w21_runinbound_92c923_profile_arg @ gateway/run_inbound_unauthorized.py:pairing_profile_arg
 * `-p <profile> ` when the store's `profile` is a non-default string, else "". */
char *w21_runinbound_92c923_profile_arg(const json_t *profile);

/* PoP: w21_runinbound_92c923_code_reply @ gateway/run_inbound_unauthorized.py:pairing_code_reply
 * The first-time sender's DM: what happened, the code's validity, the approve
 * command (profile-scoped when `profile_arg` is non-empty). */
char *w21_runinbound_92c923_code_reply(const char *platform_name, const char *code,
                                       const char *profile_arg);

/* PoP: w21_runinbound_92c923_rate_limited @ gateway/run_inbound_unauthorized.py:pairing_rate_limited_reply */
char *w21_runinbound_92c923_rate_limited(void);

/* PoP: w21_runinbound_92c923_owner_hint @ gateway/run_inbound_unauthorized.py:unauthorized_owner_hint
 * `neutralize_untrusted_inline_text(user_name, 64)`, sigil strip, then the
 * owner notice with the platform's allowlist env hint. */
char *w21_runinbound_92c923_owner_hint(const char *platform_name, const char *user_id,
                                       const char *user_name, const char *hermes_home);

/* PoP: w21_runinbound_92c923_first_time @ gateway/run_inbound_unauthorized.py:UnauthorizedOwnerNotifier.first_time
 * One drop against a persisted LRU order: `state` is the seen list as
 * [[platform, user_id], ...] oldest first.
 *   {"first":true|false,"order":[[platform,user_id],...]} */
json_t *w21_runinbound_92c923_first_time(const json_t *state, const char *platform_name,
                                         const json_t *user_id, double max_seen);

/* PoP: w21_runinbound_92c923_allowlist_env @ gateway/pairing.py:_allowlist_env_for_platform
 * The platform's allowlist env var name, or NULL when it has none / is unknown. */
const char *w21_runinbound_92c923_allowlist_env(const char *platform_name);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_RUNINBOUND_92C923_H */
