#ifndef NOC_W22_TELEGRAMLI_E4312E_H
#define NOC_W22_TELEGRAMLI_E4312E_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/platforms/telegram/telegram_live_gates.py
 * (gate w22_telegramli_e4312e).
 *
 * The adapter reload and the TELEGRAM_ALLOWED_USERS read are host seams: the
 * caller injects the owning profile's home, the two config versions and the
 * env value, and the pure decisions of the module run here. The adapter
 * mutation helpers that need a live gateway config stay upstream.
 */

/* PoP: w22_telegramli_e4312e_keys @ plugins/platforms/telegram/telegram_live_gates.py:_LIVE_GATE_EXTRA_KEYS
 * The six `extra` keys an intake verdict reads, in declaration order. */
json_t *w22_telegramli_e4312e_keys(void);

/* PoP: w22_telegramli_e4312e_allowlist_decision @ plugins/platforms/telegram/telegram_live_gates.py:env_allowlist_decision
 * `platform_gate_env("TELEGRAM_ALLOWED_USERS").strip()` with the env value injected
 * (NULL = variable unset, same stripped result as unset in Python). null when no
 * allowlist is configured, else true/false: '*' matches every id, otherwise the
 * user id must be a stripped member of the comma list. A non-string user id is
 * never equal to a member (Python only matches str members). */
json_t *w22_telegramli_e4312e_allowlist_decision(const char *user_id, const char *env_csv);

/* PoP: w22_telegramli_e4312e_refresh_needed @ plugins/platforms/telegram/telegram_live_gates.py:refresh_telegram_gates
 * `refresh_live_gate_extra`'s pre-reload decision: no baseline (or a baseline
 * without an owning home) keeps the current state, and a config version equal to
 * the connect-time baseline does too. `baseline_tag` is the non-home part of
 * `_live_gate_version` (its first element is the home); 1 means "reload". */
int w22_telegramli_e4312e_refresh_needed(const char *baseline_home,
                                         const char *baseline_tag,
                                         const char *current_tag);

/* PoP: w22_telegramli_e4312e_merge_extra @ plugins/platforms/telegram/telegram_live_gates.py:refresh_telegram_gates
 * `_reload_live_gate_extra`'s gate copy: for each live key, take the freshly
 * loaded platform extra value when present, else drop the key. Keys outside the
 * six live keys are left untouched. A missing/falsy platform record acts like a
 * fresh `PlatformConfig()` (empty extra). Returns a new object. */
json_t *w22_telegramli_e4312e_merge_extra(const json_t *extra,
                                          const json_t *platforms,
                                          const char *platform);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W22_TELEGRAMLI_E4312E_H */
