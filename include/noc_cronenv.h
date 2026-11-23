#ifndef NOC_CRONENV_H
#define NOC_CRONENV_H
/*
 * env_val is the os.environ value (NULL == unset); scope_val / home_scope_val
 * are the secret-scope values (NULL == key missing / None).
 * Returns a malloc'd string (caller frees).
 */
char *cronenv_cron_env_setting(const char *name, const char *default_val,
                               int multiplex, int has_scope,
                               const char *env_val,
                               const char *scope_val, const char *home_scope_val);
#endif /* NOC_CRONENV_H */
