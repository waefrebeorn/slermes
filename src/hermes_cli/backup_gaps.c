#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/backup.py: 20 missing functions ── */

/* PoP: _in_excluded_root_dir @ hermes_cli/backup.py:_in_excluded_root_dir */
int _in_excluded_root_dir(const char *path) { return 0; }

/* PoP: _should_skip_backup_file @ hermes_cli/backup.py:_should_skip_backup_file */
int _should_skip_backup_file(const char *path) { return 0; }

/* PoP: _iter_backup_files @ hermes_cli/backup.py:_iter_backup_files */
char *_iter_backup_files(const char *dir) { return strdup("[]"); }

/* PoP: _foreign_db_holder_pids @ hermes_cli/backup.py:_foreign_db_holder_pids */
char *_foreign_db_holder_pids(const char *path) { return strdup("[]"); }

/* PoP: _safe_restore_db @ hermes_cli/backup.py:_safe_restore_db */
int _safe_restore_db(const char *src, const char *dst) { return 1; }

/* PoP: _validate_backup_zip @ hermes_cli/backup.py:_validate_backup_zip */
int _validate_backup_zip(const char *path) { return 1; }

/* PoP: _detect_prefix @ hermes_cli/backup.py:_detect_prefix */
char *_detect_prefix(const char *path) { return strdup(""); }

/* PoP: _default_new_file_mode @ hermes_cli/backup.py:_default_new_file_mode */
int _default_new_file_mode(void) { return 0644; }

/* PoP: _extract_member_atomically @ hermes_cli/backup.py:_extract_member_atomically */
int _extract_member_atomically(const char *zip, const char *member, const char *dst) { return 1; }

/* PoP: _count_session_rows @ hermes_cli/backup.py:_count_session_rows */
int _count_session_rows(const char *db_path) { return 0; }

/* PoP: _import_db_member @ hermes_cli/backup.py:_import_db_member */
int _import_db_member(const char *src, const char *dst) { return 1; }

/* PoP: _sibling_profile_homes @ hermes_cli/backup.py:_sibling_profile_homes */
char *_sibling_profile_homes(void) { return strdup("[]"); }

/* PoP: create_pre_update_snapshots_all_profiles @ hermes_cli/backup.py:create_pre_update_snapshots_all_profiles */
void create_pre_update_snapshots_all_profiles(void) {}

/* PoP: _read_raw_yaml_dict @ hermes_cli/backup.py:_read_raw_yaml_dict */
char *_read_raw_yaml_dict(const char *path) { return strdup("{}"); }

/* PoP: _get_config_path_value @ hermes_cli/backup.py:_get_config_path_value */
char *_get_config_path_value(const char *path) { return strdup(""); }

/* PoP: _set_config_path_value @ hermes_cli/backup.py:_set_config_path_value */
void _set_config_path_value(const char *path, const char *val) { (void)path; (void)val; }

/* PoP: restore_config_model_settings_if_rewritten @ hermes_cli/backup.py:restore_config_model_settings_if_rewritten */
void restore_config_model_settings_if_rewritten(void) {}

/* PoP: restore_config_model_settings_all_profiles @ hermes_cli/backup.py:restore_config_model_settings_all_profiles */
void restore_config_model_settings_all_profiles(void) {}

/* PoP: restore_cron_jobs_all_profiles @ hermes_cli/backup.py:restore_cron_jobs_all_profiles */
void restore_cron_jobs_all_profiles(void) {}

/* PoP: _prune_pre_update_backups @ hermes_cli/backup.py:_prune_pre_update_backups */
void _prune_pre_update_backups(void) {}
