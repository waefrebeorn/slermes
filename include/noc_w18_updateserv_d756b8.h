#ifndef NOC_W18_UPDATESERV_D756B8_H
#define NOC_W18_UPDATESERV_D756B8_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* C port of hermes_cli/update_serve_resume.py candidate-command builder.
 * PoP: w18_updateserv_d756b8_candidates @ hermes_cli/update_serve_resume.py:main
 */

/* The candidate list the fresh-process adapter hands to
 * _respawn_dashboard_processes: -> JSON array of
 * {"pid":<original>, "command":[argv...], "hermes_home":string|null}.
 * `executable` (sys.executable) and the dependency/respawn hooks are injected
 * seams; the port/purpose validation and argv assembly are verbatim. */
json_t *w18_updateserv_d756b8_candidates(const char *root, const char *executable,
                                         const json_t *entries);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_UPDATESERV_D756B8_H */
