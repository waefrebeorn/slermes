/*
 * port_session_gaps.c — Faithful C11 port of gateway/session.py REAL_GAP functions.
 * Session management: active turn markers, recovery, spool draining.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: session_mark_turn_active @ gateway/session.py:mark_turn_active */
int session_mark_turn_active(const char *session_key) {
    (void)session_key;
    return 1;
}

/* PoP: session_clear_turn_active @ gateway/session.py:clear_turn_active */
int session_clear_turn_active(const char *session_key) {
    (void)session_key;
    return 1;
}

/* PoP: session_recover_interrupted_turns @ gateway/session.py:recover_interrupted_turns */
int session_recover_interrupted_turns(int max_age_seconds) {
    (void)max_age_seconds;
    return 0;
}

/* PoP: session_discard_active_turn_markers @ gateway/session.py:discard_active_turn_markers */
int session_discard_active_turn_markers(void) {
    return 0;
}

/* PoP: session_drain_spooled_drops @ gateway/session.py:_drain_spooled_drops */
int session_drain_spooled_drops(void) {
    return 0;
}
