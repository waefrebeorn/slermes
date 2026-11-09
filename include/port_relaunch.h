/*
 * port_relaunch.h -- pure argv helpers ported from hermes_cli/relaunch.py.
 *
 * Process replacement (os.execv) and executable discovery stay in Python.
 */
#ifndef PORT_RELAUNCH_H
#define PORT_RELAUNCH_H

#include <stddef.h>

/* relaunch.py:_extract_inherited_flags -- pull out the flags that carry over
 * into a self-relaunched hermes. table_flags/table_values supply the extra
 * parser-derived entries (pass NULL/0 for the static pre-argparse set only).
 * Returns a newly allocated NULL-terminated array of BORROWED pointers; free
 * the ARRAY only. */
const char **rl_extract_inherited_flags(const char *const *argv,
                                        const char *const *table_flags,
                                        const int *table_values, int table_n,
                                        int *out_count);

/* relaunch.py:build_relaunch_argv -- [bin_path] or [python, -m, hermes_cli.main],
 * then the inherited flags (when preserve_inherited), then extra_args.
 * Returns a newly allocated NULL-terminated array of borrowed pointers. */
const char **rl_build_relaunch_argv(const char *bin_path, const char *python_exe,
                                    const char *const *original_argv,
                                    int preserve_inherited,
                                    const char *const *extra_args,
                                    int *out_count);

#endif /* PORT_RELAUNCH_H */
