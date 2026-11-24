#ifndef NOC_W17_WEBSERVER_7F8B25_H
#define NOC_W17_WEBSERVER_7F8B25_H

#include "hermes_json.h"

/* C port of hermes_cli/web_server_surface.py (gate w17_webserver_7f8b25).
 * All exported symbols carry the verbatim gate key prefix.
 * Returned char* is malloc'd and caller-owned; NULL represents Python None. */

/* surface(): getattr(state,"ui_surface",None) or the gateway/dashboard fallback. */
char *w17_webserver_7f8b25_surface(const char *ui_surface, bool has_gateway_runner);

/* policy(): the full SurfacePolicy field set for the resolved surface, as JSON.
 * Returns a JSON_OBJECT (caller frees). */
json_t *w17_webserver_7f8b25_policy(const char *ui_surface, bool has_gateway_runner);

/* private_launch(): policy(state).launch_link and not state.auth_required. */
bool w17_webserver_7f8b25_private_launch(const char *ui_surface, bool has_gateway_runner,
                                        bool auth_required);

#endif
