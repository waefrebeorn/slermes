#ifndef NOC_SLACKCFG_H
#define NOC_SLACKCFG_H
#include <stddef.h>

/* getter.__name__ == "_slack_" + key */
void slackcfg_flag_getter_name(const char *key, char *buf, size_t cap);
/* Value the getter produces when self._extra_or_env_flag forwards
 * (key, env_var, strip); rendered as f"{key}|{env_var}|{strip}". */
void slackcfg_flag_getter_call(const char *key, const char *env_var, int strip,
                               char *buf, size_t cap);

void slackcfg_channel_set_getter_name(const char *key, char *buf, size_t cap);
/* Value the getter produces when self._extra_or_env_channel_set forwards
 * (key, env_var, coerce_scalar); rendered as f"{key}|{env_var}|{coerce_scalar}". */
void slackcfg_channel_set_getter_call(const char *key, const char *env_var,
                                      int coerce_scalar, char *buf, size_t cap);
#endif
