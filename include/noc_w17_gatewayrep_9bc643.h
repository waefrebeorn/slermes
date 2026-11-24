#ifndef NOC_W17_GATEWAYREP_9BC643_H
#define NOC_W17_GATEWAYREP_9BC643_H

#include "hermes_json.h"

/* Faithful C11 port of hermes_cli/gateway_replay_gap.py (gate w17_gatewayrep_9bc643). */
json_t *w17_gatewayrep_9bc643_pending_status(const json_t *pending, const char *admission);
json_t *w17_gatewayrep_9bc643_settled(const json_t *receipt);
json_t *w17_gatewayrep_9bc643_owed_reply(const char *shown, const char *final);

#endif /* NOC_W17_GATEWAYREP_9BC643_H */
