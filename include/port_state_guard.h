/*
 * port_state_guard.h -- pure predicate logic ported from
 * persistence/sqlite/_state/guard.py (the live-DB test-isolation guard).
 *
 * The env / process-walk / registry / locking halves stay in Python; these
 * are the deterministic cores, fed the values the impure callers read:
 * env strings for _running_under_pytest, argv tokens for
 * _process_looks_like_pytest, path strings for _is_production_state_db.
 */
#ifndef PORT_STATE_GUARD_H
#define PORT_STATE_GUARD_H

#include <stdbool.h>

/* _running_under_pytest -- truthiness of the three env signals (NULL models
 * an unset variable; "" is falsy exactly like os.environ.get). */
bool sg_running_under_pytest(const char *pytest_current_test,
                             const char *pytest_version,
                             const char *test_isolation_marker);

/* _process_looks_like_pytest -- the argv analysis over proc.cmdline()
 * tokens. NULL / n<=0 models an unreadable cmdline (fails open: false). */
bool sg_process_looks_like_pytest(const char *const *cmdline, int n);

/* _is_production_state_db -- <root>/state.db or <root>/profiles/<name>/state.db;
 * deeper scratch paths deliberately NOT matched. Path comparison follows the
 * LIVE Windows authority: separators normalized, '.' dropped, case-insensitive
 * component equality (the "profiles" slot test itself is case-sensitive). */
bool sg_is_production_state_db(const char *resolved, const char *root);

#endif /* PORT_STATE_GUARD_H */
