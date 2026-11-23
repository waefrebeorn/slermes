#ifndef NOC_SESSOWNER_H
#define NOC_SESSOWNER_H

/* C port of hermes_cli/sessions_owner.py. All exports sessowner_-prefixed. */
/* Full text of report_owner_failure's message (no trailing newline); malloc'd. */
char *sessowner_report_owner_failure_message(const char *exc);
int sessowner_prune_rpc_timeout(void);
/* live_owner's discovery decision: endpoint only when the found gateway is
 * "ready". Returns the borrowed `endpoint` pointer, or NULL. */
const char *sessowner_live_owner_endpoint(const char *state, const char *endpoint);
#endif
