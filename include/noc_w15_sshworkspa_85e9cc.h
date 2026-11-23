#ifndef NOC_W15_SSHWORKSPA_85E9CC_H
#define NOC_W15_SSHWORKSPA_85E9CC_H

#include "hermes_json.h"

/* PoP port of hermes_cli/ssh_workspace_fs.py (gate w15_sshworkspa_85e9cc). */

/* SshWorkspaceFs._error_code(output, fallback) -> string. */
json_t *w15_sshworkspa_85e9cc_error_code(const char *output, const char *fallback);

/* SshWorkspaceFs.normalize(path) -> string, with cwd / remote_home injected. */
json_t *w15_sshworkspa_85e9cc_normalize(const char *path, const char *cwd, const char *remote_home);

#endif /* NOC_W15_SSHWORKSPA_85E9CC_H */
