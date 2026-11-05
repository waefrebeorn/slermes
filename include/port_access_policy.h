/*
 * port_access_policy.h — pure helpers from
 * gateway/platforms/access_policy_mixin.py. The secret/env reads stay
 * unported: the resolved env values are taken as parameters.
 */
#ifndef PORT_ACCESS_POLICY_H
#define PORT_ACCESS_POLICY_H

/* _open_dm_opted_in: True when either allow-all env value, stripped +
 * lowercased, is in OPTIN_TRUTHY = {"true","1","yes"}. env0 is the
 * GATEWAY_ALLOW_ALL_USERS value, env1 the <prefix>_ALLOW_ALL_USERS value
 * (order irrelevant). */
int apm_open_dm_opted_in(const char *env0_value, const char *env1_value);

/* _entry_matches(entries, target): exact membership of target in the JSON
 * string array entries. */
int apm_entry_matches(const char *entries_json, const char *target);

/* _is_dm_allowed(sender_id): allowlist -> membership; else "open" AND the
 * opted-in flag (the env-secret read is pre-resolved into `optin`). */
int apm_is_dm_allowed(const char *dm_policy, const char *sender_id,
                      const char *allow_json, int optin);

/* _is_dm_intake_allowed(sender_id): blank principal -> False; pairing admits;
 * else delegates to _is_dm_allowed on the stripped principal. */
int apm_is_dm_intake_allowed(const char *dm_policy, const char *sender_id,
                             const char *allow_json, int optin);

/* _is_group_allowed(chat_id): allowlist -> membership; else "open". */
int apm_is_group_allowed(const char *group_policy, const char *chat_id,
                         const char *group_allow_json);

#endif /* PORT_ACCESS_POLICY_H */
