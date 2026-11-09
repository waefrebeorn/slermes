/*
 * port_kanban_db_dispatch.h -- pure helpers ported from
 * hermes_cli/kanban_db_dispatch.py.
 *
 * The sqlite / process / signal half stays in Python.
 */
#ifndef PORT_KANBAN_DB_DISPATCH_H
#define PORT_KANBAN_DB_DISPATCH_H

#include <stddef.h>

/* _wait_status_from_returncode -- (returncode & 0xFF) << 8. */
int kbd_wait_status_from_returncode(int returncode);

/* _exit_code_kind -- returns 0=clean_exit, 1=rate_limited,
 * 2=terminal_provider, 3=nonzero_exit; *out_code receives the code as given.
 * The two special codes come from the kanban package and are parameters. */
int kbd_exit_code_kind(int code, int rate_limit_code, int terminal_provider_code,
                       int *out_code);

/* _error_fingerprint -- pid N / <TS> normalization, lowercased and stripped.
 * Caller frees. */
char *kbd_error_fingerprint(const char *error_text);

/* _worker_survived_termination -- `termination` is a flat [key, value, ...]
 * array; boolean values are "1"/"0". */
int kbd_worker_survived_termination(const char *const *termination);

#endif /* PORT_KANBAN_DB_DISPATCH_H */
