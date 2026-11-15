/*
 * port_pm_environments.c — Port of pm/environments.py.
 *
 * Dependency-environment path resolution: the install key, the per-install
 * state directory, the facts/inputs paths, and the venv layout. The env-var
 * seams (dependency_home_root) and the filesystem probes (_recorded_venv,
 * base_venv) stay out — callers pass the root they already resolved.
 */

#include "port_pm_environments.h"

#include <hash.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INSTALLS_DIR "installs"
#define FACTS_FILE "facts.json"
#define INPUTS_DIR "inputs"
#define WINDOWS_SCRIPTS "Scripts"
#define POSIX_BIN "bin"
#define WINDOWS_PYTHON "python.exe"
#define POSIX_PYTHON "python"

/* Python's Path / operator: one separator, whatever the left side ends with. */
static void path_join(const char *a, const char *b, char *out, size_t outsz)
{
    size_t la = (a && *a) ? strlen(a) : 0;
    if (la && (a[la - 1] == '/' || a[la - 1] == '\\'))
        snprintf(out, outsz, "%s%s", a, b);
    else
        snprintf(out, outsz, "%s/%s", a ? a : "", b);
}

/* PoP: install_key @ pm/environments.py:install_key */
void pm_install_key(const char *canonical_project_root, char *out,
                    size_t outsz)
{
    if (!out || outsz == 0)
        return;
    out[0] = '\0';
    if (!canonical_project_root)
        return;
    char *hex = hash_sha256_hex((const unsigned char *)canonical_project_root,
                                strlen(canonical_project_root));
    if (!hex)
        return;
    snprintf(out, outsz, "%.16s", hex);
    free(hex);
}

/* PoP: installs_root @ pm/environments.py:installs_root */
void pm_installs_root(const char *dependency_root, char *out, size_t outsz)
{
    path_join(dependency_root, INSTALLS_DIR, out, outsz);
}

/* PoP: install_state_dir @ pm/environments.py:install_state_dir */
void pm_install_state_dir(const char *canonical_project_root,
                          const char *dependency_root, char *out, size_t outsz)
{
    char key[32];
    pm_install_key(canonical_project_root, key, sizeof(key));
    char root[4096];
    pm_installs_root(dependency_root, root, sizeof(root));
    path_join(root, key, out, outsz);
}

/* PoP: runtime_facts_path @ pm/environments.py:runtime_facts_path */
void pm_runtime_facts_path(const char *canonical_project_root,
                           const char *dependency_root, char *out,
                           size_t outsz)
{
    char state[4096];
    pm_install_state_dir(canonical_project_root, dependency_root, state,
                         sizeof(state));
    path_join(state, FACTS_FILE, out, outsz);
}

/* PoP: activation_inputs_dir @ pm/environments.py:activation_inputs_dir */
void pm_activation_inputs_dir(const char *canonical_project_root,
                              const char *dependency_root, char *out,
                              size_t outsz)
{
    char state[4096];
    pm_install_state_dir(canonical_project_root, dependency_root, state,
                         sizeof(state));
    path_join(state, INPUTS_DIR, out, outsz);
}

/* PoP: venv_bin_dir @ pm/environments.py:venv_bin_dir */
void pm_venv_bin_dir(const char *venv, int windows, char *out, size_t outsz)
{
    path_join(venv, windows ? WINDOWS_SCRIPTS : POSIX_BIN, out, outsz);
}

/* PoP: venv_python @ pm/environments.py:venv_python */
void pm_venv_python(const char *venv, int windows, char *out, size_t outsz)
{
    char bin[4096];
    pm_venv_bin_dir(venv, windows, bin, sizeof(bin));
    path_join(bin, windows ? WINDOWS_PYTHON : POSIX_PYTHON, out, outsz);
}
