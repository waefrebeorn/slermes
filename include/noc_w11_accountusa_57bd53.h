#ifndef NOC_W11_ACCOUNTUSA_57BD53_H
#define NOC_W11_ACCOUNTUSA_57BD53_H

/* C API for the agent/account_usage_cache.py port (gate w11_accountusa_57bd53). */
#include "hermes_json.h"

/* _key(provider, identity_id, base_url) with injected hermes_home_key -> [4 strings] */
json_t *w11_accountusa_57bd53_key(const char *home_key, const char *provider,
                                   const char *identity_id, const char *base_url);
/* _identity_id_for(provider, entry) with injected codex principal -> str (malloc'd).
 * principal == NULL means no decoded principal; otherwise a 2-element array. */
char *w11_accountusa_57bd53_identity_id(const char *provider, const char *runtime_api_key,
                                        const char *entry_id, const json_t *principal);
/* snapshot_is_stale(snapshot) with injected clock/validity -> bool */
bool w11_accountusa_57bd53_snapshot_is_stale(bool valid, bool bad_timestamp,
                                              double fetched_at, double now);

#endif /* NOC_W11_ACCOUNTUSA_57BD53_H */
