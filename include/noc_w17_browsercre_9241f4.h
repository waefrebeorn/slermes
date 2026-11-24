#ifndef NOC_W17_BROWSERCRE_9241F4_H
#define NOC_W17_BROWSERCRE_9241F4_H

#include "hermes_json.h"

/* Faithful C11 port of hermes_cli/browser_credential.py (gate w17_browsercre_9241f4).
 * The web_server._SESSION_TOKEN dependency is injected as the `credential` argument
 * so the port stays pure and the oracle can drive it deterministically. */
json_t *w17_browsercre_9241f4_generation_of(const char *credential);
json_t *w17_browsercre_9241f4_current_if(const char *recorded, int recorded_is_null,
                                         const char *credential);
json_t *w17_browsercre_9241f4_is_current(const char *recorded, int recorded_is_null,
                                         const char *credential);

#endif /* NOC_W17_BROWSERCRE_9241F4_H */
