/*
 * noc_w34_holders_028cc0.h - C surface for upstream
 * persistence/sqlite/_state/holders.py (gate w34_holders_028cc0).
 *
 * holders.py answers one question: does a foreign process still hold the
 * active (or unlinked) state.db / -wal / -shm generation?  Everything that
 * touches the machine -- /proc, psutil, the Windows Restart Manager, the
 * sqlite lock probe, hermes_constants' profile layout, the profile selector
 * and the recovery-docs URL -- stays in the caller as a seam.  This file owns
 * only what the values decide:
 *
 *   * argv predicates  - which interpreter/script/module an argv runs, hence
 *                        whether it is a Hermes process at all;
 *   * argv selection   - --hermes-home / HERMES_HOME= / --profile / -p, token
 *                        exact, last wins;
 *   * path identity    - normcase/normpath/abspath identity, the DB family,
 *                        and the /proc " (deleted)" suffix;
 *   * home scoping     - whether an argv provably belongs to a DIFFERENT
 *                        instance (issue #92401);
 *   * row shaping      - the operator-facing holder labels and the refusal
 *                        block built from a holder list.
 *
 * Path semantics follow the host exactly as Python's os.path does: ntpath
 * (drive letters, '\\' separator, case folding) under _WIN32, posixpath
 * elsewhere.  Two seams are injected as plain values so the pure decision is
 * testable and identical on both sides:
 *
 *   install_root / our_profile  <- _store_install_layout(this_home)
 *                                  (hermes_constants.named_profile_home)
 *   argv                        <- _read_proc_argv(pid) / psutil
 *
 * A NULL const char * argument means Python's None.
 */
#ifndef NOC_W34_HOLDERS_028CC0_H
#define NOC_W34_HOLDERS_028CC0_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _looks_like_python_executable: "python3.11.exe" / "pypy3" -> true. */
int w34_holders_028cc0_looks_like_python_executable(const char *program);

/* _python_execution_target: {"kind": "module"|"script", "value": str} or null. */
json_t *w34_holders_028cc0_python_execution_target(const json_t *argv);

/* _looks_like_hermes: argv runs a supported Hermes executable/module/script. */
int w34_holders_028cc0_looks_like_hermes(const json_t *argv);

/* _argv_flag_value: last "--flag X" / "--flag=X" value for one of *flags*. */
json_t *w34_holders_028cc0_argv_flag_value(const json_t *argv, const json_t *flags);

/* _argv_env_home: last "HERMES_HOME=<path>" argv token, or null. */
json_t *w34_holders_028cc0_argv_env_home(const json_t *argv);

/* _sqlite_family: [base, base-wal, base-shm]. */
json_t *w34_holders_028cc0_sqlite_family(const char *base);

/* _norm_path: os.path.normcase(os.path.normpath(value)). */
json_t *w34_holders_028cc0_norm_path(const char *value);

/* canonical_sqlite_path: normcase(abspath(strip " (deleted)")). */
json_t *w34_holders_028cc0_canonical_sqlite_path(const char *path);

/* _argv_path_tokens: [[argv index, normalized absolute path], ...]. */
json_t *w34_holders_028cc0_argv_path_tokens(const json_t *argv);

/* _names_other_profile: token under <install root>/profiles/<name != ours>. */
int w34_holders_028cc0_names_other_profile(const char *normalized,
                                           const char *install_root,
                                           const char *our_profile);

/* _argv_home_selection: "ours" | "other" | NULL (json null). */
json_t *w34_holders_028cc0_argv_home_selection(const json_t *argv,
                                               const char *this_home,
                                               const char *install_root,
                                               const char *our_profile);

/* _argv_scoped_to_other_home: argv proves a DIFFERENT instance (#92401). */
int w34_holders_028cc0_argv_scoped_to_other_home(const json_t *argv,
                                                 const char *db_path,
                                                 const char *install_root,
                                                 const char *our_profile);

/* describe_holder_pid: "PID 123 (hermes gateway run)"; argv null -> unavailable. */
json_t *w34_holders_028cc0_describe_holder_pid(long pid, const json_t *argv);

/* held_store_refusal: the refusal block for *holders*, or null when empty.
 * holders: [[pid, target], ...]; argv is unavailable for these labels. */
json_t *w34_holders_028cc0_held_store_refusal(const char *db_path,
                                              const json_t *holders,
                                              const char *command,
                                              const char *profile_arg,
                                              const char *docs_url,
                                              const char *force_hint);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W34_HOLDERS_028CC0_H */
