#ifndef NOC_W10_BOTMODEDm_EBBA71_H
#define NOC_W10_BOTMODEDm_EBBA71_H
#include "hermes_json.h"

/* PoP port of tools/bot_mode_dm_chain.py (gate w10_botmodedm_ebba71). */
int   w10_botmodedm_ebba71_definitely_not_sent(json_t *ack);
char *w10_botmodedm_ebba71_peer_key(const char *peer_name, const char *peer_profile,
                                    json_t *turn_author, const char *peer_url);
#endif /* NOC_W10_BOTMODEDm_EBBA71_H */
