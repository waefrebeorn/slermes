#ifndef PORT_ENVIRONMENTS_H
#define PORT_ENVIRONMENTS_H

#include <stddef.h>

/* pm/environments.py — dependency-environment path resolution.
 *
 * The install key, the state directory, the venv layout: every launcher and
 * `scripts/run-in-hermes-env` path needs these, and 50 reference modules import
 * them. Paths are joined with "/" (valid on every platform the C tree runs on);
 * the oracle normalizes Python's "\" for comparison.
 *
 * The env-var seams are taken as parameters: `dependency_root` is what
 * dependency_home_root() resolves to, and `windows` is the layout flag
 * venv_bin_dir takes. */

/* PoP: install_key @ pm/environments.py:install_key */
void pm_install_key(const char *canonical_project_root, char *out,
                    size_t outsz);

/* PoP: installs_root @ pm/environments.py:installs_root */
void pm_installs_root(const char *dependency_root, char *out, size_t outsz);

/* PoP: install_state_dir @ pm/environments.py:install_state_dir */
void pm_install_state_dir(const char *canonical_project_root,
                          const char *dependency_root, char *out, size_t outsz);

/* PoP: runtime_facts_path @ pm/environments.py:runtime_facts_path */
void pm_runtime_facts_path(const char *canonical_project_root,
                           const char *dependency_root, char *out,
                           size_t outsz);

/* PoP: activation_inputs_dir @ pm/environments.py:activation_inputs_dir */
void pm_activation_inputs_dir(const char *canonical_project_root,
                              const char *dependency_root, char *out,
                              size_t outsz);

/* PoP: venv_bin_dir @ pm/environments.py:venv_bin_dir */
void pm_venv_bin_dir(const char *venv, int windows, char *out, size_t outsz);

/* PoP: venv_python @ pm/environments.py:venv_python */
void pm_venv_python(const char *venv, int windows, char *out, size_t outsz);

#endif /* PORT_ENVIRONMENTS_H */
