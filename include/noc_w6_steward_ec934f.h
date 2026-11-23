#ifndef NOC_W6_STEWARD_EC934F_H
#define NOC_W6_STEWARD_EC934F_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C port of hermes_cli/steward.py (gate w6_steward_ec934f).
 * Filesystem reads are explicit seams: `content` is the text of
 * install-stamp.json (NULL == missing/unreadable) and `has_git` says whether
 * <root>/.git exists. JSON null stands in for Python None. */

/* PoP: w6_steward_ec934f_steward_update_message @ hermes_cli/steward.py:steward_update_message */
char *w6_steward_ec934f_steward_update_message(const char *steward);
/* PoP: w6_steward_ec934f_steward_uninstall_message @ hermes_cli/steward.py:steward_uninstall_message
 * platform NULL stands in for the host default (the oracle runs on win32). */
char *w6_steward_ec934f_steward_uninstall_message(const char *steward, const char *platform);
/* PoP: w6_steward_ec934f_read_install_stamp @ hermes_cli/steward.py:read_install_stamp */
struct json_t *w6_steward_ec934f_read_install_stamp(const char *content);
/* PoP: w6_steward_ec934f_sealed_steward @ hermes_cli/steward.py:sealed_steward */
char *w6_steward_ec934f_sealed_steward(int has_git, const char *content);
/* PoP: w6_steward_ec934f_is_bundled_payload @ hermes_cli/steward.py:is_bundled_payload */
int w6_steward_ec934f_is_bundled_payload(const char *content);
/* PoP: w6_steward_ec934f_classify_install @ hermes_cli/steward.py:classify_install */
struct json_t *w6_steward_ec934f_classify_install(int has_git, const char *content);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W6_STEWARD_EC934F_H */
