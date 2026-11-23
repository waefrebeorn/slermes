/*
 * bot_peer_notices.c — C port of gateway/bot_peer_notices.py.
 * PoP: botnotice_automatic_notices_allowed @ gateway/bot_peer_notices.py:automatic_notices_allowed
 *
 * automatic_notices_allowed(source) -> getattr(source, "is_bot", False) is not True
 * Anything other than a real True (missing attr, False, 1, "yes", None, ...) is allowed.
 */
#include "noc_botnotice.h"

int botnotice_automatic_notices_allowed(int is_bot_present, int is_bot_value) {
    return !(is_bot_present && is_bot_value) ? 1 : 0;
}
