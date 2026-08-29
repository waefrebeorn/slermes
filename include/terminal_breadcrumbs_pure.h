#ifndef HERMES_TERMINAL_BREADCRUMBS_PURE_H
#define HERMES_TERMINAL_BREADCRUMBS_PURE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pure helpers ported from hermes_cli/terminal_breadcrumbs.py.
 * tbr_sanitize returns a malloc'd string (free()). */

char *tbr_sanitize(const char *raw);
int   tbr_is_stale_payload(double ts, double now);

/* Derived identity for the current terminal — tty path or first present
 * multiplexer env var.  Returns a malloc'd string (free()), or NULL when
 * no terminal identity can be determined (best-effort: caller skips).
 * Faithful to Python's get_terminal_id(). */
char *tbr_get_terminal_id(void);

/* Record this terminal's live session is ``session_id``.
 * Synchronous, best-effort, never raises (no-op when disabled / empty /
 * no terminal identity). Mirrors write_breadcrumb(). */
void tbr_write_breadcrumb(const char *session_id, const char *cwd);

/* Best-effort removal of breadcrumbs older than the staleness window. */
void tbr_prune_stale(const char *dir, double now);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_TERMINAL_BREADCRUMBS_PURE_H */
