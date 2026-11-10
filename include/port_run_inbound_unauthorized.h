#ifndef SLERMES_PORT_RUN_INBOUND_UNAUTHORIZED_H
#define SLERMES_PORT_RUN_INBOUND_UNAUTHORIZED_H

/* pairing_profile_arg(profile): "-p <profile> " when profile is a non-empty
 * string != "default", else "". Returns malloc'd string, caller frees. */
char *run_inbound_pairing_profile_arg(const char *profile);

#endif /* SLERMES_PORT_RUN_INBOUND_UNAUTHORIZED_H */
