/* noc_w19_profileide_c2f0d9.h - public C API for persistence/housekeeping/profile_identity.py. */
#ifndef NOC_W19_PROFILEIDE_C2F0D9_H
#define NOC_W19_PROFILEIDE_C2F0D9_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_profileide_c2f0d9_profile_namespace @ persistence/housekeeping/profile_identity.py:profile_namespace */
json_t *w19_profileide_c2f0d9_profile_namespace(const char *profile);

/* PoP: w19_profileide_c2f0d9_rekey_as_dict @ persistence/housekeeping/profile_identity.py:ProfileRekeyCounts.as_dict
 * (_present: one key per facet the store has, in dataclass field order, None left out). */
json_t *w19_profileide_c2f0d9_rekey_as_dict(json_t *counts);

/* PoP: w19_profileide_c2f0d9_purge_as_dict @ persistence/housekeeping/profile_identity.py:ProfilePurgeCounts.as_dict */
json_t *w19_profileide_c2f0d9_purge_as_dict(json_t *counts);

/* PoP: w19_profileide_c2f0d9_collision @ persistence/housekeeping/profile_identity.py:ProfileIdentityCollision.__init__
 * -> {"str": message, "entity": "profile", "entity_id": new_name} */
json_t *w19_profileide_c2f0d9_collision(const char *new_name, const char *message);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_PROFILEIDE_C2F0D9_H */
