#ifndef NOC_W15_BACKUPKEEP_A55382_H
#define NOC_W15_BACKUPKEEP_A55382_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_backupkeep_a55382_host_env_key @ hermes_cli/backup_keep_host.py:host_env_key */
int w15_backupkeep_a55382_host_env_key(const char *name);
/* PoP: w15_backupkeep_a55382__env_name @ hermes_cli/backup_keep_host.py:_env_name */
struct json_t *w15_backupkeep_a55382_env_name(const char *line);
/* PoP: w15_backupkeep_a55382__host_lines @ hermes_cli/backup_keep_host.py:_host_lines */
struct json_t *w15_backupkeep_a55382_host_lines(const char *text);
/* PoP: w15_backupkeep_a55382_merge_env @ hermes_cli/backup_keep_host.py:merge_env */
struct json_t *w15_backupkeep_a55382_merge_env(const char *archive_text, const char *host_text);
/* PoP: w15_backupkeep_a55382_merge_config @ hermes_cli/backup_keep_host.py:merge_config */
struct json_t *w15_backupkeep_a55382_merge_config(const struct json_t *archive_cfg, const struct json_t *host_cfg);
/* PoP: w15_backupkeep_a55382_merge_auth @ hermes_cli/backup_keep_host.py:merge_auth */
struct json_t *w15_backupkeep_a55382_merge_auth(const struct json_t *archive_store, const struct json_t *host_store);
/* PoP: w15_backupkeep_a55382__home_parts @ hermes_cli/backup_keep_host.py:_home_parts */
struct json_t *w15_backupkeep_a55382_home_parts(const struct json_t *parts);
/* PoP: w15_backupkeep_a55382__is_auth_store @ hermes_cli/backup_keep_host.py:_is_auth_store */
int w15_backupkeep_a55382_is_auth_store(const struct json_t *parts);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_BACKUPKEEP_A55382_H */
