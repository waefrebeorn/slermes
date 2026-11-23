/*
 * session_bot_state.c - Live state of a Bot Chat (C11 port).
 *
 * PoP: botstate_bot_chat_state @ gateway/session_bot_state.py:bot_chat_state
 *
 * The live function reads a SessionAuthority object graph; the gate supplies
 * that graph as pre-resolved scalars from the harness fixture (retiring,
 * runner._draining, presence of a live session, handle.execution_state and
 * the prompt kinds from live.controls.snapshot).  The branch order is the
 * exact live order: paused -> idle -> running -> waiting_* -> working.
 */
#include "noc_botstate.h"

#include <string.h>

const char *botstate_bot_chat_state(int retiring, int draining, int live,
                                    const char *exec_state,
                                    const char *const *kinds, size_t nkinds) {
    if (retiring || draining) return "paused";
    if (!live) return "idle";
    if (!exec_state || strcmp(exec_state, "running")) return "idle";
    int has_approval = 0, has_clarify = 0;
    for (size_t i = 0; i < nkinds; i++) {
        if (!kinds[i]) continue;
        if (!strcmp(kinds[i], "approval")) has_approval = 1;
        if (!strcmp(kinds[i], "clarify")) has_clarify = 1;
    }
    if (has_approval) return "waiting_approval";
    if (has_clarify) return "waiting_clarify";
    return "working";
}
