/*
 * web_server_app.c — C home for upstream Hermes hermes_cli/web_server_app.py.
 *
 * PoP: w12_webserver_a3baf6_app_lifespan @ hermes_cli/web_server_app.py:app_lifespan
 *
 * The only branch in app_lifespan is the pure dispatch:
 *   lifespan = http_lifespan if getattr(app.state, "gateway_runner", None) is not None
 *              else standalone_lifespan
 * A JSON null (or a missing key) is Python None -> standalone; every other
 * JSON value is non-None -> http.
 */
#include "hermes_json.h"

#include "noc_w12_webserver_a3baf6.h"

json_t *w12_webserver_a3baf6_select_lifespan(const json_t *gateway_runner) {
    if (!gateway_runner || gateway_runner->type == JSON_NULL)
        return json_string("standalone");
    return json_string("http");
}

void hermes_cli_web_server_app_port_pending(void) { (void)0; }

