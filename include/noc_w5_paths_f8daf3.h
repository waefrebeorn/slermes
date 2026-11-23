#ifndef NOC_W5_PATHS_F8DAF3_H
#define NOC_W5_PATHS_F8DAF3_H
#include <stddef.h>

/* Public C API for pm/paths.py (gate w5_paths_f8daf3).
 * The store/lockfile location depends on __file__ and the process environment;
 * those values are injected at the gate so the path-selection logic can be
 * checked faithfully without touching the host filesystem. */

/* PoP: pmpaths_install_root @ pm/paths.py:install_root
 * env_value NULL/"" means HERMES_INSTALL_ROOT is unset/empty. */
int pmpaths_install_root(const char *repo_root, const char *env_value, char **out);

/* PoP: pmpaths_install_stamp_path @ pm/paths.py:install_stamp_path
 * resolves_to_repo: whether Path(project_root).resolve() == repo_root(). */
int pmpaths_install_stamp_path(const char *project_root, int resolves_to_repo,
                               const char *install_root, char **out);

/* PoP: pmpaths_partials_root @ pm/paths.py:partials_root */
int pmpaths_partials_root(const char *default_root, char **out);

/* PoP: pmpaths_facts_path @ pm/paths.py:facts_path */
int pmpaths_facts_path(const char *store_root, char **out);

#endif /* NOC_W5_PATHS_F8DAF3_H */
