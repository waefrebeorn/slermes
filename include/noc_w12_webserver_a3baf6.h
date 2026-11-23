#ifndef NOC_W12_WEBSERVER_A3BAF6_H
#define NOC_W12_WEBSERVER_A3BAF6_H

#include "hermes_json.h"

/* PoP port of hermes_cli/web_server_app.py (gate w12_webserver_a3baf6). */

/* app_lifespan's pure dispatch: gateway_runner is not None -> "http", else "standalone". */
json_t *w12_webserver_a3baf6_select_lifespan(const json_t *gateway_runner);

#endif /* NOC_W12_WEBSERVER_A3BAF6_H */
