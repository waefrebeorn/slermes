/*
 * noc_w25_updateowni_0f1bd1.h - C surface for upstream
 * hermes_cli/update_owning_install.py (gate w25_updateowni_0f1bd1).
 *
 * An install's in-tree venv can end up importing another checkout's code, so
 * ``hermes update`` must go back to the install whose interpreter it is running
 * on.  The filesystem, ``sys.prefix``, ``PYTHONPATH`` and the child process are
 * seams; this header owns which checkout owns this interpreter, the PM
 * generation record that proves it, and the retarget message/argv.
 */
#ifndef NOC_W25_UPDATEOWNI_0F1BD1_H
#define NOC_W25_UPDATEOWNI_0F1BD1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W25_UPDATEOWNI_0F1BD1_KEY_CHARS 16
#define W25_UPDATEOWNI_0F1BD1_MODULE "hermes_cli.main"

/* pm.environments.install_key: sha256(utf8(canonical path)).hexdigest()[:16].
 * `path` is the already-resolved canonical path (resolution is the seam). */
json_t *w25_updateowni_0f1bd1_install_key(const char *path);

/* owning_install_root(diag) -> the owning checkout, or null.
 * diag: {"prefix", "base_prefix", "project_root", "main_py",
 *        "pm_owner": str|null, "pythonpath": [str, ...]} */
json_t *w25_updateowni_0f1bd1_owning_root(const json_t *diag);

/* _pm_environment_owner(diag) ->
 * "<installs>/<key>/environments/<gen>/venv" -> the checkout recorded in
 * state/inputs/.project-root, when its install key matches the state dir.
 * diag: {"venv", "record": str|null, "main_py"} */
json_t *w25_updateowni_0f1bd1_pm_owner(const json_t *diag);

/* retarget(owner, prefix, project_root, executable, args) ->
 * {"lines": [...], "pythonpath", "cwd", "argv": [...]} */
json_t *w25_updateowni_0f1bd1_retarget(const json_t *req);

/* The venv names and module path this decision is tuned with. */
json_t *w25_updateowni_0f1bd1_constants(void);

#ifdef __cplusplus
}
#endif
#endif
