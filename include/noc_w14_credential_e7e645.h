#ifndef NOC_W14_CREDENTIAL_E7E645_H
#define NOC_W14_CREDENTIAL_E7E645_H

#include "hermes_json.h"

/* Faithful C11 port of upstream agent/credential_pool_admin.py. */

json_t *w14_credential_e7e645_resolve_target(const char *target, const json_t *entries);
json_t *w14_credential_e7e645_remove_index_ex(const json_t *entries, long long index,
                                              const char *current_id);

#endif /* NOC_W14_CREDENTIAL_E7E645_H */
