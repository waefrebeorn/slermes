/**
 * @file hermes_skills_hub.h
 * @brief L12: Browse.sh skills catalog source.
 *
 * Fetches and searches the browse.sh skills catalog (169+ browser
 * automation skills). Provides a simple HTTP-based catalog source.
 */
#ifndef HERMES_SKILLS_HUB_H
#define HERMES_SKILLS_HUB_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

/* ================================================================
 *  Constants
 * ================================================================ */
#define SKILLS_HUB_CATALOG_URL "https://browse.sh/api/skills"
#define SKILLS_HUB_SOURCE_ID "browse-sh"
#define SKILLS_HUB_SOURCE_ID_WELLKNOWN "wellknown"
#define SKILLS_HUB_MAX_RESULTS 50
#define SKILLS_HUB_MAX_SKILLS 64   /* max skills per source catalog (was 512: 512×3.3KB×16 sources ≈ 26MB of .bss) */
#define SKILLS_HUB_CACHE_TTL_SEC 300 /* 5 min in-memory cache */
#define SKILLS_HUB_MAX_SOURCES 16   /* max registered skill sources */

/* Source type identifiers */
#define SKILLS_HUB_SRC_BROWSESH 0
#define SKILLS_HUB_SRC_WELLKNOWN 1

/* ================================================================
 *  Hub metadata constants (v326)
 * ================================================================ */
#define HUB_LOCK_VERSION 1
#define HUB_LOCK_FILENAME "lock.json"
#define HUB_TAPS_FILENAME "taps.json"
#define HUB_AUDIT_FILENAME "audit.log"
#define HUB_QUARANTINE_DIRNAME "quarantine"
#define HUB_INDEX_CACHE_DIRNAME "index-cache"
#define HUB_INSTALLED_MAX 128
#define HUB_TAPS_MAX 64
#define HUB_LINE_MAX 4096

/* Trust levels (matching Python skills_hub.py) */
#define HUB_TRUST_BUILTIN "builtin"
#define HUB_TRUST_TRUSTED "trusted"
#define HUB_TRUST_COMMUNITY "community"

/* Scan verdicts */
#define HUB_SCAN_CLEAN "clean"
#define HUB_SCAN_WARN "warn"
#define HUB_SCAN_BLOCKED "blocked"

/* Audit log action types */
#define HUB_AUDIT_INSTALL "install"
#define HUB_AUDIT_UNINSTALL "uninstall"
#define HUB_AUDIT_SCAN "scan"
#define HUB_AUDIT_TAP_ADD "tap-add"
#define HUB_AUDIT_TAP_REMOVE "tap-remove"

/* ================================================================
 *  Types
 * ================================================================ */
typedef struct {
    char slug[128];
    char name[128];
    char title[256];
    char description[1024];
    char source_url[512];
    char provider[64];     /* matches Python SkillMeta.extra.provider */
    char category[64];
    char tags[1024];        /* comma-separated */
    char recommended_method[32];
    int  install_count;
    bool needs_proxy;
} hub_skill_meta_t;

typedef struct {
    hub_skill_meta_t skills[SKILLS_HUB_MAX_SKILLS];
    int count;
    bool loaded;
} skills_catalog_t;

/* A registered skill source provides skills to the unified search. */
typedef struct {
    char source_id[64];
    int  type;      /* SKILLS_HUB_SRC_BROWSESH, _WELLKNOWN, etc. */
    skills_catalog_t catalog;
} skill_source_t;

/* ================================================================
 *  Hub metadata types (v326)
 * ================================================================ */

/* A single installed skill entry in lock.json */
typedef struct {
    char name[128];
    char source[64];
    char identifier[128];
    char trust_level[32];
    char scan_verdict[32];
    char content_hash[128];
    char install_path[4096];
    char files[4096];     /* comma-separated relative paths */
    char installed_at[64];
    char updated_at[64];
} hub_installed_skill_t;

/* A tap entry */
typedef struct {
    char repo[256];
    char path[256];
} hub_tap_entry_t;

/* ================================================================
 *  API
 * ================================================================ */

/**
 * Fetch the browse.sh skills catalog (with in-memory cache).
 * Returns true on success (cached or fresh), false on error.
 */
bool skills_hub_fetch_catalog(void);

/**
 * Search cached catalog by query string.
 * Returns up to limit results. query="" returns all.
 */
int skills_hub_search(const char *query, hub_skill_meta_t *results, int limit);

/**
 * Get a skill by slug. Returns true if found.
 */
bool skills_hub_get_by_slug(const char *slug, hub_skill_meta_t *out);

/**
 * Clear the in-memory cache (force next fetch to go to network).
 */
void skills_hub_clear_cache(void);

/**
 * Get a human-readable summary string. Caller must free().
 */
char *skills_hub_summary(void);

/* v323: Multi-source support — register well-known static skills */
bool skills_hub_register_static(const hub_skill_meta_t *skills, int count, const char *source_id);
int  skills_hub_unified_search(const char *query, hub_skill_meta_t *results, int limit);
/* Pure: keep only results whose provider matches (exact, case-insensitive). */
bool hub_filter_results_by_provider(hub_skill_meta_t *results, int count, const char *provider);
int  skills_hub_source_count(void);
const char *skills_hub_source_name(int index);

/* v324: Install/uninstall skills from sources */
bool skills_hub_install_from_url(const char *url);
bool skills_hub_install_from_source(const char *source_id, const char *identifier);
bool skills_hub_uninstall(const char *skill_name);
int  skills_hub_list_installed(char names[][128], int max_count);
bool skills_hub_is_installed(const char *skill_name);

/* v326: Hub metadata management — lock file, taps, audit, path validation */

/* Path validation (port of Python _validate_skill_name, _normalize_bundle_path) */
bool hub_validate_skill_name(const char *name);
/* Pure: GitHub tap-repo -> provider label (owner/repo, case-insensitive). */
const char *github_provider_for(const char *repo);
/* Faithful _validate_skill_name returning the normalized name (out buffer). */
bool hub_normalize_skill_name(const char *name, char *out, size_t out_sz);
bool hub_normalize_lock_install_path(const char *install_path, const char *skill_name,
                                     char *out, size_t out_sz);
/* Faithful _validate_install_parent_path returning the normalized path. */
bool hub_validate_install_parent_path(const char *category, char *out, size_t out_len);

/* Hub directory structure */
bool hub_ensure_dirs(void);

/* Lock file operations (skills/.hub/lock.json) */
bool hub_lock_record_install(const hub_installed_skill_t *entry);
bool hub_lock_record_uninstall(const char *skill_name);
bool hub_lock_get_installed(const char *skill_name, hub_installed_skill_t *out);
int  hub_lock_list_installed(hub_installed_skill_t *entries, int max_count);

/* Taps management (skills/.hub/taps.json) */
bool hub_taps_add(const char *repo, const char *path);
bool hub_taps_remove(const char *repo);
int  hub_taps_list(hub_tap_entry_t *entries, int max_count);

/* Audit log */
bool hub_append_audit_log(const char *action, const char *skill_name,
                           const char *source, const char *trust_level,
                           const char *verdict, const char *extra);

/* v328+: Bundle path validation, quarantine, symlink detection */
bool hub_validate_bundle_rel_path(const char *rel_path);
bool hub_is_path_redirect(const char *path);
bool hub_quarantine_write(const char *skill_name, const char *filename,
                           const char *content, size_t content_len);

/* v332+: Bundle content hash */
typedef struct {
    const char *filename;
    const unsigned char *content;
    size_t content_len;
} hub_bundle_file_t;
char *hub_bundle_content_hash(const hub_bundle_file_t *files, size_t count);
/* Port of Python: _skill_meta_to_dict */
json_t *hub_skill_meta_to_json(const hub_skill_meta_t *meta);

#endif /* HERMES_SKILLS_HUB_H */
