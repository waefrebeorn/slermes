#ifndef ACP_MODES_H
#define ACP_MODES_H

#include "hermes_json.h"

const char *acp_modes_default_mode(void);
const char *acp_modes_default_edit_approval_policy(void);
const char *acp_modes_edit_approval_policy_config_id(void);
const char *acp_modes_policy_for_mode(const char *mode, const char *fallback);
const char *acp_modes_mode_for_policy(const char *policy);
json_t *acp_modes_table(void);
json_t *acp_modes_mode_to_policy(void);
json_t *acp_modes_policy_to_mode(void);

#endif /* ACP_MODES_H */
