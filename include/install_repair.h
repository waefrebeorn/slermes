/*
 * install_repair.h — faithful C11 port of hermes_cli/_install_repair.py
 * (dependency install execution shared between early recovery and full
 * recovery).
 *
 * PoP (Python -> C):
 *   _is_windows                        -> install_repair_is_windows
 *   _is_termux_env                     -> install_repair_is_termux_env
 *   _resolve_install_target            -> install_repair_resolve_install_target
 *   _venv_scripts_dir                  -> install_repair_venv_scripts_dir
 *   _load_console_script_names         -> install_repair_load_console_script_names
 *   _quarantine_running_hermes_exe     -> install_repair_quarantine_running_hermes_exe
 *   _restore_quarantined_exes          -> install_repair_restore_quarantined_exes
 *   _run_install_cmd                   -> install_repair_run_install_cmd
 *   _load_installable_optional_extras  -> install_repair_load_installable_optional_extras
 *   run_core_install                   -> install_repair_run_core_install
 *   bump_marker_attempts               -> install_repair_bump_marker_attempts
 *
 * Side effects (subprocess execution, uv lookup, sys.executable) route
 * through injectable function pointers / parameters so hosts can substitute
 * fakes; the defaults do the real thing.
 */
#ifndef HERMES_INSTALL_REPAIR_H
#define HERMES_INSTALL_REPAIR_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Injectable hooks (NULL => real default). */
typedef char *(*install_repair_find_uv_fn)(void);
/* subprocess.run(cmd, cwd=root, check=True) analog: return 0 on success,
 * nonzero => CalledProcessError. */
typedef int (*install_repair_run_cmd_fn)(char *const *argv, size_t n,
                                         const char *cwd);

extern install_repair_find_uv_fn install_repair_find_uv_hook;
extern install_repair_run_cmd_fn install_repair_run_cmd_hook;
extern const char *install_repair_sys_executable; /* default: "python3" */

bool install_repair_is_windows(void);

/* env: JSON object of environment vars, or NULL for the real environ. */
bool install_repair_is_termux_env(const json_t *env);

/* Result of _resolve_install_target. */
typedef struct {
    char **prefix;      /* install command prefix, e.g. [uv, "pip"] */
    size_t n_prefix;
    bool has_env;       /* Python returns (prefix, None) when no uv */
    char *virtual_env;  /* VIRTUAL_ENV value when has_env */
    bool strip_python_vars; /* Termux: PYTHONPATH/PYTHONHOME popped */
} install_repair_target_t;

install_repair_target_t *install_repair_resolve_install_target(const char *root);
void install_repair_target_free(install_repair_target_t *t);

/* Project venv Scripts/bin dir when present; malloc'd path or NULL. */
char *install_repair_venv_scripts_dir(const char *root);

/* "[project.scripts]" names from pyproject.toml (tomllib analog).
 * Returns malloc'd string array (+count); NULL on any failure/absence. */
char **install_repair_load_console_script_names(const char *root, size_t *n);

/* Windows-only quarantine of live hermes*.exe shims. On non-Windows this is
 * a faithful no-op returning 0 moved pairs (mirrors the Python early-out). */
typedef struct { char *original; char *quarantined; } install_repair_move_t;
size_t install_repair_quarantine_running_hermes_exe(const char *scripts_dir,
                                                    install_repair_move_t **out);
void install_repair_restore_quarantined_exes(install_repair_move_t *moved,
                                             size_t n);

/* Optional extras referenced by a dependency group ("all"/"termux-all"). */
char **install_repair_load_installable_optional_extras(const char *root,
                                                       const char *group,
                                                       size_t *n);

/* Increment the attempts counter inside a marker JSON file. Returns the new
 * count; never fails (corrupt/missing restart at 1). */
int install_repair_bump_marker_attempts(const char *marker_path);

/* run_core_install decision ladder. Records every attempted command and the
 * printed advisory lines into out_commands/out_printed (JSON arrays owned by
 * caller-allocated json_t objects). Uses injectable run hook when set. */
void install_repair_run_core_install(const char *root,
                                     json_t *out_commands,
                                     json_t *out_printed);

#ifdef __cplusplus
}
#endif
#endif /* HERMES_INSTALL_REPAIR_H */
