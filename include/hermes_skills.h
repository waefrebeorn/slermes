#ifndef HERMES_SKILLS_H
#define HERMES_SKILLS_H

/* Focused skills-system header.
 * Extracted from the hermes.h umbrella so skills subsystem consumers
 * (tools/skills.c, agent/prompt_builder.c, cli/cli_cmd_skills.c,
 * api_server.c, web_dashboard.c) no longer pull in the god header. */

#include "hermes_core_types.h"

/* P179-P188: Skills system depth */

/* Skill provenance/origin */
typedef enum {
    SKILL_ORIGIN_LOCAL,
    SKILL_ORIGIN_HUB,
    SKILL_ORIGIN_BUNDLE,
    SKILL_ORIGIN_UNKNOWN,
} skill_origin_t;

/* Skill metadata (full) */
typedef struct {
    char  name[128];              /* skill name (directory name) */
    char  path[HERMES_PATH_MAX];  /* full path to skill directory */
    char  version[32];            /* semver from SKILL.md frontmatter */
    char  author[256];            /* author field */
    char  description[512];       /* description */
    char  category[128];          /* category tag */
    char  tags[1024];             /* comma-separated tags */
    char  dependencies[1024];     /* comma-separated skill deps */
    char  bundles[1024];          /* comma-separated bundle aliases */
    skill_origin_t origin;        /* provenance: local/hub/bundle */
    time_t last_updated;          /* mtime of SKILL.md */
    time_t last_used;             /* last time skill was invoked */
    int   usage_count;            /* how many times skill was used */
    bool  validated;              /* passed frontmatter validation */
    char  validation_error[256];  /* validation error if any */
} skill_meta_t;

/* Cache entry for LRU cache */
typedef struct skill_cache_entry_t {
    char  name[128];
    char *content;                /* full SKILL.md content */
    skill_meta_t meta;
    time_t loaded_at;
    struct skill_cache_entry_t *prev;
    struct skill_cache_entry_t *next;
} skill_cache_entry_t;

/* Skill cache (doubly-linked list for LRU) */
typedef struct {
    skill_cache_entry_t *head;    /* most recently used */
    skill_cache_entry_t *tail;    /* least recently used */
    size_t count;
    size_t max_entries;           /* 0 = unlimited */
} skill_cache_t;

/* Skill scanning result (list of metadata) */
typedef struct {
    skill_meta_t *skills;
    size_t count;
    size_t capacity;
} skill_list_t;

/* Dependency node for resolution */
typedef struct skill_dep_node_t {
    char  name[128];
    char  version[32];
    bool  resolved;
    struct skill_dep_node_t **deps; /* array of dependency names */
    size_t deps_count;
} skill_dep_node_t;

/* Search result */
typedef struct {
    char  name[128];
    char  path[HERMES_PATH_MAX];
    float score;                  /* relevance score 0.0-1.0 */
} skill_search_result_t;

/* P179: Scan skills directory — recursive, extract metadata, cache */
skill_list_t *skills_scan_all(void);
void skills_scan_free(skill_list_t *list);

/* P180: Validate skill frontmatter */
bool skill_validate(const char *skill_name, char *error_out, size_t err_sz);
bool skill_validate_all(void);

/* P181: Get/set skill provenance */
skill_origin_t skill_get_origin(const char *skill_name);
bool skill_set_origin(const char *skill_name, skill_origin_t origin);

/* P182: Sync skills from git-based hub */
bool skill_sync_from_hub(const char *hub_url, const char *branch, char *log_out, size_t log_sz);

/* P183: Bundle management */
bool skill_bundle_create(const char *bundle_name, const char *skills_csv);
bool skill_bundle_delete(const char *bundle_name);
skill_list_t *skill_bundle_get_skills(const char *bundle_name);

/* P184: Usage tracking */
void skill_record_usage(const char *skill_name);
int  skill_get_usage_count(const char *skill_name);
time_t skill_get_last_used(const char *skill_name);
void skill_get_recommendations(skill_meta_t *out, size_t *count, size_t max_count);

/* P185: Cache management */
bool skill_cache_init(size_t max_entries);
void skill_cache_destroy(void);
bool skill_cache_preload(const char *skill_name);
void skill_cache_evict(const char *skill_name);
const char *skill_cache_get(const char *skill_name);
size_t skill_cache_count(void);

/* P186: Search skills */
skill_search_result_t *skill_search(const char *query, const char *tag_filter,
                                     size_t *result_count, size_t max_results);
void skill_search_free(skill_search_result_t *results, size_t count);

/* L12: Browse.sh skills hub — search and install */
skill_search_result_t *skill_search_hub(const char *query,
                                         size_t *result_count, size_t max_results);
void skill_search_hub_free(skill_search_result_t *results, size_t count);
bool skill_install_from_hub(const char *slug, char *error_out, size_t err_sz);

/* P187: Curator — stale detection and auto-update */
bool skill_curator_run(char *report_out, size_t report_sz);
bool skill_curator_set_stale_days(int days);
int  skill_curator_get_stale_days(void);

/* P188: Dependency resolution */
bool skill_deps_resolve(const char *skill_name,
                         char ordered[][128], size_t *count, size_t max);
char **skill_deps_get_missing(const char *skill_name, size_t *count);
bool skill_deps_validate_order(const char ordered[][128], size_t count);

#endif /* HERMES_SKILLS_H */
