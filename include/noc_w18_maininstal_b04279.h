#ifndef NOC_W18_MAININSTAL_B04279_H
#define NOC_W18_MAININSTAL_B04279_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: maininstal_is_windows_npm_path @ hermes_cli/main_install_repair.py:_is_windows_npm_path */
int maininstal_is_windows_npm_path(const char *npm_path);

/* PoP: maininstal_quarantine_stamp_ms @ hermes_cli/main_install_repair.py:_quarantine_stamp_ms
 * The .old.<unix-ms> stamp from a filename, or JSON null. */
struct json_t *maininstal_quarantine_stamp_ms(const char *stale_name);

/* PoP: maininstal_resolve_update_branch @ hermes_cli/main_install_repair.py:_resolve_update_branch
 * Normalize args.branch to a non-empty stripped name (default "main"). Caller frees. */
char *maininstal_resolve_update_branch(const char *branch);

/* PoP: maininstal_filter_pending_shim_renames @ hermes_cli/main_install_repair.py:_filter_pending_shim_renames
 * Returns {"kept":[str,...], "removed":int}; caller frees. */
struct json_t *maininstal_filter_pending_shim_renames(const struct json_t *entries,
                                                      const struct json_t *shims);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W18_MAININSTAL_B04279_H */
