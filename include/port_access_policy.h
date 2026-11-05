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

#endif /* PORT_ACCESS_POLICY_H */
