#ifndef NOC_W17_GATEWAYCON_0AB9ED_H
#define NOC_W17_GATEWAYCON_0AB9ED_H

#include "hermes_json.h"

/* Faithful C11 port of hermes_cli/gateway_connection.py (gate w17_gatewaycon_0ab9ed).
 * Epoch arguments are decimal strings; NULL reproduces Python's None. */
json_t *w17_gatewaycon_0ab9ed_lost_before(int is_exception, const char *connection_epoch,
                                          const char *epoch);
json_t *w17_gatewaycon_0ab9ed_runs_route(const json_t *info, const char *model, const char *provider);
/* apply_event on a fresh SessionState; returns a summary of the mutated fields. */
json_t *w17_gatewaycon_0ab9ed_apply_event(const char *kind, const json_t *payload);

#endif /* NOC_W17_GATEWAYCON_0AB9ED_H */
