#ifndef PORT_GATEWAY_PROFILE_ROUTING_H
#define PORT_GATEWAY_PROFILE_ROUTING_H

#include "hermes_json.h"

/* Pure discriminator helpers of Python: gateway/profile_routing.py.
 * The route parse/match and the WhatsApp alias expansion are runtime/coupled
 * and NOT ported. */

/* ProfileRoute.specificity: 2*guild + 4*chat + 8*thread + 16*user. */
int pr_specificity(int guild, int chat, int thread, int user);

/* _bot_profile_key: strip; None when empty or "default", else the name. */
char *pr_bot_profile_key(const char *name);

/* _coerce_route_id: None/str pass through; int-not-bool -> decimal str; bool ->
 * "True"/"False"; other numbers -> %.17g. malloc'd or NULL. */
char *pr_coerce_route_id(const json_t *value);

/* _is_whatsapp_non_user_chat: ends with @g.us / @broadcast / @newsletter. */
int pr_is_whatsapp_non_user_chat(const char *chat_id);

#endif /* PORT_GATEWAY_PROFILE_ROUTING_H */
