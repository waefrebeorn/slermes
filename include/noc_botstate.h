#ifndef NOC_BOTSTATE_H
#define NOC_BOTSTATE_H
#include <stddef.h>
/* Pre-resolved SessionAuthority state.  Returns one of the static strings
 * "paused", "idle", "working", "waiting_approval", "waiting_clarify". */
/* PoP: botstate_bot_chat_state @ gateway/session_bot_state.py:bot_chat_state */
const char *botstate_bot_chat_state(int retiring, int draining, int live,
                                    const char *exec_state,
                                    const char *const *kinds, size_t nkinds);
#endif
