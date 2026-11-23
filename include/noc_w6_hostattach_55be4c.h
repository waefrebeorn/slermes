#ifndef NOC_W6_HOSTATTACH_55BE4C_H
#define NOC_W6_HOSTATTACH_55BE4C_H
#include "hermes_json.h"

/* C home for upstream Hermes gateway/host_attach.py (gate w6_hostattach_55be4c).
 * Only the pure identity/served-set/message helpers are ported; process
 * liveness, control-socket polling and locking stay in Python. */

/* PoP: w6_hostattach_served_from_identity @ gateway/host_attach.py:_served_from_identity
 * Returns a malloc'd NULL-terminated array of malloc'd profile-name strings. */
char **w6_hostattach_served_from_identity(const json_t *identity, size_t *out_n);
/* PoP: w6_hostattach_free_strings @ gateway/host_attach.py:<module> */
void w6_hostattach_free_strings(char **arr, size_t n);

/* PoP: w6_hostattach_serves @ gateway/host_attach.py:HostGateway.serves */
int w6_hostattach_serves(int served_known, char **profiles, size_t n,
                         const char *wanted);

/* PoP: w6_hostattach_standalone_rescan_message @ gateway/host_attach.py:standalone_rescan_message */
void w6_hostattach_standalone_rescan_message(const char *profile,
                                             char *out, size_t cap);

#endif /* NOC_W6_HOSTATTACH_55BE4C_H */
