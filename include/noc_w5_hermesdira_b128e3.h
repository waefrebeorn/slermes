#ifndef NOC_W5_HERMESDIRA_B128E3_H
#define NOC_W5_HERMESDIRA_B128E3_H

#include "hermes_json.h"

/* C port of the pure string/constant leaves of hermes_dir_acl.py.
 * All exports are hermesdira_-prefixed. Returned char* values are malloc'd
 * and caller-owned (free()). */

/* _strip_win32_prefix: drop a leading \\?\ or rewrite \\?\UNC\ to \\. */
char *hermesdira_strip_win32_prefix(const char *path);

/* unusable_dir_remedy: the one-line repair, host selected by `windows`. */
char *hermesdira_unusable_dir_remedy(const char *path, int windows);

/* The Win32 token/attribute constants declared at module scope. */
json_t *hermesdira_constants(void);

#endif /* NOC_W5_HERMESDIRA_B128E3_H */
