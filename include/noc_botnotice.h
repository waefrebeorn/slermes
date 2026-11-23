#ifndef NOC_BOTNOTICE_H
#define NOC_BOTNOTICE_H

/* C port of gateway/bot_peer_notices.py.
 * PoP: botnotice_automatic_notices_allowed @ gateway/bot_peer_notices.py:automatic_notices_allowed
 *
 * Python: getattr(source, "is_bot", False) is not True.
 * Inputs model the source attribute as tri-state: is_bot_present (attribute
 * exists) and is_bot_value (a real Python True). Only present+True blocks.
 */
int botnotice_automatic_notices_allowed(int is_bot_present, int is_bot_value);

#endif /* NOC_BOTNOTICE_H */
