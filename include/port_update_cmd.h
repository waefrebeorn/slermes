/*
 * port_update_cmd.h -- pure helpers ported from hermes_cli/update_cmd.py.
 *
 * The git/pip execution half stays in Python. Most of update_cmd.py's defs are
 * `stop_for_relaunch()` shims and are deliberately NOT ported.
 */
#ifndef PORT_UPDATE_CMD_H
#define PORT_UPDATE_CMD_H

#include <stddef.h>

/* _called_process_error_cmd_parts -- the argv of a CalledProcessError. `argv`
 * is the already-tokenised form; `cmdline` the single-string form (NULL when
 * the error carried an argv). `posix` is 0 on Windows, 1 on POSIX. Caller frees
 * each string and the array. */
char **uc_cmd_parts(const char *const *argv, const char *cmdline, int posix,
                    int *out_count);

/* _called_process_error_is_git -- the failed subprocess was git itself. */
int uc_is_git(const char *const *argv);

/* _called_process_error_is_python_dep_install -- a uv/pip/ensurepip install. */
int uc_is_dep_install(const char *const *argv);

/* _format_update_failure_stage -- the human stage label. Caller frees. */
char *uc_failure_stage(const char *const *argv);

/* _should_zip_fallback_on_update_error -- ZIP fallback is for Windows git file
 * I/O breakage only. `is_cpe` / `is_windows` / `checkout_untouched` stand in for
 * the three externals. */
int uc_zip_fallback(const char *const *argv, int is_cpe, int is_windows,
                    int checkout_untouched);

#endif /* PORT_UPDATE_CMD_H */
