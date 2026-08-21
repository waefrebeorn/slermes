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

#ifdef __cplusplus
}
#endif

#endif /* HERMES_TERMINAL_BREADCRUMBS_PURE_H */
