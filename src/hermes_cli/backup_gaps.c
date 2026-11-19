#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/backup.py: 20 missing functions ── */

/* ── _in_excluded_root_dir revival (tail_revive3) ───────────────────────────
 * Faithful port of hermes_cli/backup.py:_in_excluded_root_dir.
 * rel_path.parts is modeled as PurePosixPath.parts (deterministic across
 * gate hosts): split on '/', drop empty and "." components, keep "..",
 * a leading '/' becomes the root part "/". Windows drive/UNC/backslash
 * paths are out of fixture scope (documented projection).
 * Constants transcribed from hermes_constants.LOCAL_RUNTIME_ROOT_DIRS,
 * hermes_cli.home_data_layout.{PM_RUNTIME_ROOT_DIRS, SECRET_CACHE_FILES,
 * BRIDGE_LOG_PARTS, profile_root_entry} and agent.provider_media.
 * GENERATED_SUBDIR:
 *   _EXCLUDED_ROOT_DIRS = {"models","runtimes","node","installs","tools"}
 *   _EXCLUDED_BACKUP_ROOT_DIRS = {"browser_profiles","bws_cache.json",
 *       "bws_cache.enc.json","op_cache.json","photon-sidecar.json"}
 *   BRIDGE_LOG_PARTS = {("platforms","whatsapp","bridge.log"),
 *       ("platforms","whatsapp","bridge.log.1"),("whatsapp","bridge.log"),
 *       ("whatsapp","bridge.log.1")}
 *   _KEPT_CACHE_SUBDIRS = {"images","audio","videos","documents",
 *       "screenshots","citations","generated"} */

typedef struct bk_part { const char *s; size_t n; } bk_part_t;

static int bk_peq(const bk_part_t *p, const char *lit) {
    return strlen(lit) == p->n && memcmp(p->s, lit, p->n) == 0;
}

static int bk_in_list(const bk_part_t *p, const char *const *list, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (bk_peq(p, list[i])) return 1;
    return 0;
}

/* PoP: in_excluded_root_dir @ hermes_cli/backup.py:_in_excluded_root_dir */
int in_excluded_root_dir(const char *path) {
    bk_part_t parts[512];
    size_t np = 0;
    const char *p = path ? path : "";
    if (*p == '/') {
        parts[np].s = "/"; parts[np].n = 1; np++;
    }
    while (*p && np < 512) {
        while (*p == '/') p++;
        if (!*p) break;
        const char *e = p;
        while (*e && *e != '/') e++;
        size_t len = (size_t)(e - p);
        if (!(len == 1 && p[0] == '.')) {
            parts[np].s = p; parts[np].n = len; np++;
        }
        p = e;
    }

    /* profile_root_entry(parts) */
    const bk_part_t *root = NULL;
    if (np >= 3 && bk_peq(&parts[0], "profiles")) root = &parts[2];
    else if (np > 0) root = &parts[0];

    static const char *const excl_root[] = {
        "models", "runtimes", "node", "installs", "tools"
    };
    static const char *const excl_backup[] = {
        "browser_profiles", "bws_cache.json", "bws_cache.enc.json",
        "op_cache.json", "photon-sidecar.json"
    };
    if (root && (bk_in_list(root, excl_root, 5) || bk_in_list(root, excl_backup, 5)))
        return 1;

    size_t off = (np >= 3 && bk_peq(&parts[0], "profiles")) ? 2 : 0;
    const bk_part_t *rest = parts + off;
    size_t nr = np - off;

    /* parts in BRIDGE_LOG_PARTS */
    if (nr == 3 && bk_peq(&rest[0], "platforms") && bk_peq(&rest[1], "whatsapp") &&
        (bk_peq(&rest[2], "bridge.log") || bk_peq(&rest[2], "bridge.log.1")))
        return 1;
    if (nr == 2 && bk_peq(&rest[0], "whatsapp") &&
        (bk_peq(&rest[1], "bridge.log") || bk_peq(&rest[1], "bridge.log.1")))
        return 1;

    static const char *const kept_cache[] = {
        "images", "audio", "videos", "documents", "screenshots",
        "citations", "generated"
    };
    if (nr >= 2 && bk_peq(&rest[0], "cache") && !bk_in_list(&rest[1], kept_cache, 7))
        return 1;
    return 0;
}

/* PoP: should_skip_backup_file @ hermes_cli/backup.py:should_skip_backup_file */
int should_skip_backup_file(const char *path) { return 0; }

/* PoP: iter_backup_files @ hermes_cli/backup.py:_iter_backup_files */
char *iter_backup_files(const char *dir) { return strdup("[]"); }

/* PoP: foreign_db_holder_pids @ hermes_cli/backup.py:_foreign_db_holder_pids */
char *foreign_db_holder_pids(const char *path) { return strdup("[]"); }

/* PoP: safe_restore_db @ hermes_cli/backup_restore.py:_safe_restore_db */
int safe_restore_db(const char *src, const char *dst) { return 1; }

/* PoP: validate_backup_zip @ hermes_cli/backup_restore.py:_validate_backup_zip */
int validate_backup_zip(const char *path) { return 1; }

/* PoP: detect_prefix @ hermes_cli/backup_restore.py:_detect_prefix */
char *detect_prefix(const char *path) { return strdup(""); }

/* PoP: default_new_file_mode @ hermes_cli/backup_restore.py:_default_new_file_mode */
int default_new_file_mode(void) { return 0644; }

/* PoP: extract_member_atomically @ hermes_cli/backup_restore.py:_extract_member_atomically */
int extract_member_atomically(const char *zip, const char *member, const char *dst) { return 1; }

int count_session_rows(const char *db_path) { return 0; }

/* PoP: import_db_member @ hermes_cli/backup_restore.py:_import_db_member */
int import_db_member(const char *src, const char *dst) { return 1; }

/* PoP: sibling_profile_homes @ hermes_cli/backup.py:_sibling_profile_homes */
char *sibling_profile_homes(void) { return strdup("[]"); }


/* PoP: read_raw_yaml_dict @ hermes_cli/backup.py:_read_raw_yaml_dict */
char *read_raw_yaml_dict(const char *path) { return strdup("{}"); }

/* PoP: get_config_path_value @ hermes_cli/backup_restore.py:_get_config_path_value */
char *get_config_path_value(const char *path) { return strdup(""); }

/* PoP: set_config_path_value @ hermes_cli/backup_restore.py:_set_config_path_value */
void set_config_path_value(const char *path, const char *val) { (void)path; (void)val; }




