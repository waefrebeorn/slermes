#ifndef NOC_W14_TERMINALTO_E936FA_H
#define NOC_W14_TERMINALTO_E936FA_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C home for upstream Hermes tools/terminal_tool_config.py (gate w14_terminalto_e936fa).
 * Every exported symbol is prefixed with the gate name. All functions return a
 * newly allocated json_t that the caller must free with json_free(). */

/* PoP: w14_terminalto_e936fa_profile_container_key @ tools/terminal_tool_config.py:profile_container_key */
struct json_t *w14_terminalto_e936fa_profile_container_key(const struct json_t *profile);

/* PoP: w14_terminalto_e936fa_is_host_cwd @ tools/terminal_tool_config.py:_is_host_cwd */
struct json_t *w14_terminalto_e936fa_is_host_cwd(const struct json_t *path);

/* PoP: w14_terminalto_e936fa_is_windows_drive_path @ tools/terminal_tool_config.py:_is_windows_drive_path */
struct json_t *w14_terminalto_e936fa_is_windows_drive_path(const struct json_t *path);

/* PoP: w14_terminalto_e936fa_host_path_key @ tools/terminal_tool_config.py:_host_path_key */
struct json_t *w14_terminalto_e936fa_host_path_key(const struct json_t *path);

/* PoP: w14_terminalto_e936fa_translate_mounted_host_path @ tools/terminal_tool_config.py:translate_mounted_host_path */
struct json_t *w14_terminalto_e936fa_translate_mounted_host_path(const struct json_t *path,
                                                                const struct json_t *host_root,
                                                                const struct json_t *container_root);

/* PoP: w14_terminalto_e936fa_is_unusable_container_cwd @ tools/terminal_tool_config.py:_is_unusable_container_cwd
 * mounted_host is injected as NULL (the Python default), so the host-mount equality
 * branch is a no-op in this port, exactly as in the live default call. */
struct json_t *w14_terminalto_e936fa_is_unusable_container_cwd(const struct json_t *cwd);

/* PoP: w14_terminalto_e936fa_cwd_follows_host_mount @ tools/terminal_tool_config.py:cwd_follows_host_mount */
struct json_t *w14_terminalto_e936fa_cwd_follows_host_mount(const struct json_t *cwd,
                                                           const struct json_t *mount);

/* PoP: w14_terminalto_e936fa_tenv_bool @ tools/terminal_tool_config.py:_tenv_bool
 * Explicit env_value seam: the scope-aware TERMINAL_* read is injected from the
 * fixture (Windows CRT setenv("") unsets, Python os.environ[name]="" keeps it). */
struct json_t *w14_terminalto_e936fa_tenv_bool(const struct json_t *env_value);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_TERMINALTO_E936FA_H */
