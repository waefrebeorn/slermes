#ifndef NOC_W12_BACKUPREST_0CD8E1_H
#define NOC_W12_BACKUPREST_0CD8E1_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_backuprest_0cd8e1_validate_backup_zip @ hermes_cli/backup_restore.py:_validate_backup_zip
 * names: zip member names; returns {"ok":bool,"reason":str}. */
struct json_t *w12_backuprest_0cd8e1_validate_backup_zip(const struct json_t *names);

/* PoP: w12_backuprest_0cd8e1_detect_prefix @ hermes_cli/backup_restore.py:_detect_prefix
 * names: zip member names; returns the prefix to strip ("" if none). */
struct json_t *w12_backuprest_0cd8e1_detect_prefix(const struct json_t *names);

/* PoP: w12_backuprest_0cd8e1_get_config_path_value @ hermes_cli/backup_restore.py:_get_config_path_value */
struct json_t *w12_backuprest_0cd8e1_get_config_path_value(const struct json_t *data,
                                                          const struct json_t *dotted);

/* PoP: w12_backuprest_0cd8e1_set_config_path_value @ hermes_cli/backup_restore.py:_set_config_path_value
 * Returns a fresh, modified copy of data. */
struct json_t *w12_backuprest_0cd8e1_set_config_path_value(const struct json_t *data,
                                                          const struct json_t *dotted,
                                                          const struct json_t *value);

/* PoP: w12_backuprest_0cd8e1_is_store_file @ hermes_cli/backup_restore.py:_is_store_file
 * name / resolved_name are the projections of Path.name and Path.resolve().name. */
struct json_t *w12_backuprest_0cd8e1_is_store_file(const char *name,
                                                   const char *resolved_name);

/* PoP: w12_backuprest_0cd8e1_store_file_names @ hermes_cli/backup_restore.py:_STORE_FILE_NAMES */
struct json_t *w12_backuprest_0cd8e1_store_file_names(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W12_BACKUPREST_0CD8E1_H */
