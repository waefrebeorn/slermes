#ifndef NOC_W22_FEISHUADMI_EB8884_H
#define NOC_W22_FEISHUADMI_EB8884_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/platforms/feishu/feishu_admission_diagnostics.py
 * (gate w22_feishuadmi_eb8884).
 *
 * One operator-facing decision: does this group drop come from the untouched
 * code default (policy 'allowlist' with an empty FEISHU_ALLOWED_USERS) and so
 * deserve the one WARNING, or did the operator configure the deny?
 */

/* PoP: w22_feishuadmi_eb8884_empty_allowlist_drop_warning @ plugins/platforms/feishu/feishu_admission_diagnostics.py:empty_allowlist_drop_warning
 * `empty_allowlist_drop_warning(chat_id=..., group_rules=..., default_group_policy=...,
 *  allowed_group_users=...)` -> the WARNING text, or null for None.
 *
 * `chat_id in group_rules` follows the mapping containment Python performs:
 * object keys, array elements, or a substring for a string.
 * `default_group_policy != 'allowlist'` is a plain string compare (NULL reads
 * as ""), and `allowed_group_users` is judged by Python truthiness.
 */
json_t *w22_feishuadmi_eb8884_empty_allowlist_drop_warning(const char *chat_id,
                                                           const json_t *group_rules,
                                                           const char *default_group_policy,
                                                           const json_t *allowed_group_users);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_FEISHUADMI_EB8884_H */
