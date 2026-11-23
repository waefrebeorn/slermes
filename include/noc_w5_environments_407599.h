#ifndef NOC_W5_ENVIRONMENTS_407599_H
#define NOC_W5_ENVIRONMENTS_407599_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w5_environments_407599_venv_bin_dir @ pm/environments.py:venv_bin_dir
 * windows: -1 = None (host default, nt on Windows), 0 = False, 1 = True.
 * Returns a malloc'd path string. Caller frees. */
char *w5_environments_407599_venv_bin_dir(const char *venv, int windows);

/* PoP: w5_environments_407599_venv_python @ pm/environments.py:venv_python
 * windows: -1 = None, 0 = False, 1 = True. Returns malloc'd path; caller frees. */
char *w5_environments_407599_venv_python(const char *venv, int windows);

/* PoP: w5_environments_407599_fish_quote @ pm/environments.py:_fish_quote
 * Returns a malloc'd single-quoted fish string. Caller frees. */
char *w5_environments_407599_fish_quote(const char *value);

/* PoP: w5_environments_407599_shell_exports @ pm/environments.py:shell_exports
 * dialect is "sh" or "fish". Returns a malloc'd script string. Caller frees. */
char *w5_environments_407599_shell_exports(const struct json_t *env, const char *dialect);

/* PoP: w5_environments_407599_install_key @ pm/environments.py:install_key
 * Explicit seam: `canonical` is the already-resolved project path (avoids
 * filesystem/cwd dependence). Returns malloc'd 16-char sha256 prefix. */
char *w5_environments_407599_install_key_ex(const char *canonical);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W5_ENVIRONMENTS_407599_H */
