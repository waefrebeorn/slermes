/*
 * plugin_guard.h — PoP of tools/plugin_guard.py (plugin install scanner).
 * See plugin_guard.c for the full Python -> C symbol map.
 */
#ifndef HERMES_TOOLS_PLUGIN_GUARD_H
#define HERMES_TOOLS_PLUGIN_GUARD_H

#include <stddef.h>

struct json_t;
typedef struct json_t json_t;

extern const char *PLUGIN_GUARD_SCANNER_VERSION;
extern const int PG_MAX_PLUGIN_FILE_COUNT;
extern const int PG_MAX_PLUGIN_TOTAL_SIZE_KB;
extern const int PG_MAX_PLUGIN_SINGLE_FILE_KB;

typedef struct {
    char pattern_id[64];
    char severity[16];
    char category[24];
    char file[256];
    int  line;
    char match[512];
    char description[256];
} pg_finding_t;

typedef struct { pg_finding_t *f; int n; int cap; } pg_finding_list_t;

typedef struct {
    char skill_name[128];
    char source[128];
    char trust_level[16];
    char verdict[16];
    pg_finding_t *findings;
    int finding_count;
    char summary[512];
} pg_plugin_scan_result_t;

int  plugin_guard__is_excluded(const char *rel_parts[], size_t n);
void plugin_guard__filter_findings(const pg_finding_list_t *in,
                                   const char *rel_path,
                                   pg_finding_list_t *out);
pg_finding_list_t *plugin_guard__check_plugin_structure(const char *dir);
const char *plugin_guard_determine_verdict(const pg_finding_list_t *findings);

void pg_scan_file(const char *filepath, const char *rel_path,
                  pg_finding_list_t *out);

pg_plugin_scan_result_t *plugin_guard_scan_plugin(const char *plugin_dir,
                                                  const char *source);
void plugin_guard_scan_result_free(pg_plugin_scan_result_t *r);

void should_allow_plugin_install(const pg_plugin_scan_result_t *result,
                                 int force, int *allowed, char *reason,
                                 size_t reason_sz);

const char *const *plugin_guard_excluded_dirs(void);
const char *const *plugin_guard_code_extensions(void);
int  plugin_guard__pattern_exempt_on_code(const char *pattern_id);

#endif /* HERMES_TOOLS_PLUGIN_GUARD_H */
