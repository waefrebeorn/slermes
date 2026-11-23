#ifndef NOC_W10_RUNTIMEPAT_34ADF5_H
#define NOC_W10_RUNTIMEPAT_34ADF5_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w10_runtimepat_34adf5_install_key @ pm/environments.py:install_key
 * sha256(utf8(canonical)) hexdigest()[:16]. `canonical` is the already-resolved
 * path string (Path.resolve() seam). Caller frees. */
json_t *w10_runtimepat_34adf5_install_key(const char *canonical);

/* PoP: w10_runtimepat_34adf5_venv_bin_dir @ pm/environments.py:venv_bin_dir */
json_t *w10_runtimepat_34adf5_venv_bin_dir(const char *venv, int windows);
/* PoP: w10_runtimepat_34adf5_venv_python @ pm/environments.py:venv_python */
json_t *w10_runtimepat_34adf5_venv_python(const char *venv, int windows);
/* PoP: w10_runtimepat_34adf5_site_packages @ pm/environments.py:site_packages
 * `windows` and the interpreter `major`/`minor` are injected seams for
 * os.name and venv_python_version(). Caller frees. */
json_t *w10_runtimepat_34adf5_site_packages(const char *venv, int windows, int major, int minor);

/* PoP: w10_runtimepat_34adf5_install_state_dir @ pm/environments.py:install_state_dir
 * `home_root` is dependency_home_root() and `key` is install_key(project). */
json_t *w10_runtimepat_34adf5_install_state_dir(const char *home_root, const char *key);
/* PoP: w10_runtimepat_34adf5_runtime_facts_path @ pm/environments.py:runtime_facts_path */
json_t *w10_runtimepat_34adf5_runtime_facts_path(const char *home_root, const char *key);

/* PoP: w10_runtimepat_34adf5_cli_command_name @ pm/environments.py:cli_command_name
 * `command` is the resolved launcher path (payload probe seam), or null/empty. */
json_t *w10_runtimepat_34adf5_cli_command_name(const char *command);

/* PoP: w10_runtimepat_34adf5_shell_exports @ pm/environments.py:shell_exports
 * Compose an env dict (JSON object, insertion ordered) as shell export lines. */
json_t *w10_runtimepat_34adf5_shell_exports(const json_t *env, const char *dialect);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_RUNTIMEPAT_34ADF5_H */
