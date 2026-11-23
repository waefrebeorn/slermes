#ifndef NOC_W12_BACKUPSTAG_5F272C_H
#define NOC_W12_BACKUPSTAG_5F272C_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_backupstag_5f272c_marker @ hermes_cli/backup_staging.py:_marker */
struct json_t *w12_backupstag_5f272c_marker(const char *name);

/* PoP: w12_backupstag_5f272c_is_backup_name @ hermes_cli/backup_staging.py:_NAME.fullmatch */
struct json_t *w12_backupstag_5f272c_is_backup_name(const char *name);

/* PoP: w12_backupstag_5f272c_is_legacy_name @ hermes_cli/backup_staging.py:_LEGACY_NAME.fullmatch */
struct json_t *w12_backupstag_5f272c_is_legacy_name(const char *name);

/* PoP: w12_backupstag_5f272c_stage_entries @ hermes_cli/backup_staging.py:_stage_entries */
struct json_t *w12_backupstag_5f272c_stage_entries(const struct json_t *entries);

/* PoP: w12_backupstag_5f272c_is_backup_staging @ hermes_cli/backup_staging.py:is_backup_staging
 * Filesystem state (plain-ness, lease bytes, directory entries) is injected from
 * the fixture; no real disk access is performed here. */
struct json_t *w12_backupstag_5f272c_is_backup_staging(const struct json_t *spec);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W12_BACKUPSTAG_5F272C_H */
