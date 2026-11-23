#ifndef NOC_W8_OWNERIDENTI_8D7DCB_H
#define NOC_W8_OWNERIDENTI_8D7DCB_H

/*
 * Public C API for the port of gateway/owner_identity.py (gate w8_owneridenti_8d7dcb).
 * All exported symbols are owneridenti_-prefixed to avoid collisions.
 * Dynamic host facts are injected as explicit fixture seams: host_subject is
 * owner_worker_subject(), extra_logins is list_session_providers(), and `rule`
 * is the OwnerRule the live owner_rule() builder would supply.
 */
#include "hermes_json.h"

/* PoP: owneridenti_login_provider @ gateway/owner_identity.py:_login_provider */
json_t *owneridenti_login_provider(const char *provider, const json_t *extra_logins);

/* PoP: owneridenti_principal_subject @ gateway/owner_identity.py:principal_subject */
json_t *owneridenti_principal_subject(const json_t *identity, const char *host_subject,
                                      const json_t *extra_logins);

/* PoP: owneridenti_same_owner @ gateway/owner_identity.py:same_owner */
json_t *owneridenti_same_owner(const char *stored, const char *subject, const json_t *rule);

/* PoP: owneridenti_owners_agree @ gateway/owner_identity.py:owners_agree */
json_t *owneridenti_owners_agree(const char *first, const char *second, const json_t *rule);

#endif /* NOC_W8_OWNERIDENTI_8D7DCB_H */
