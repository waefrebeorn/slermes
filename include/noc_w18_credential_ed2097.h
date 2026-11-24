#ifndef NOC_W18_CREDENTIAL_ED2097_H
#define NOC_W18_CREDENTIAL_ED2097_H

#include "hermes_json.h"

/* C home for upstream Hermes agent/credential_auth_recovery.py (state-free helpers).
 * counts are [[provider, entry_id, state], ...]; entries are pool entry dicts. */

json_t *w18_credential_ed2097_nous_refreshed(const json_t *counts);
json_t *w18_credential_ed2097_entry_for(const json_t *entries, const char *credential_id,
                                        const char *api_key_hint);
json_t *w18_credential_ed2097_episode(const json_t *counts, const char *provider,
                                      const json_t *entry_id, double now);
json_t *w18_credential_ed2097_constants(void);

#endif /* NOC_W18_CREDENTIAL_ED2097_H */
