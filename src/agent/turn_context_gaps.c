#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/turn_context.py: 5 missing functions ── */

/* PoP: _preflight_request_tokens @ agent/turn_context.py:_preflight_request_tokens */
int _preflight_request_tokens(void) { return 0; }

/* PoP: _agent_stale_thinking_on_wire @ agent/turn_context.py:_agent_stale_thinking_on_wire */
int _agent_stale_thinking_on_wire(void) { return 0; }

/* PoP: _maybe_title_session_at_turn_start @ agent/turn_context.py:_maybe_title_session_at_turn_start */
void _maybe_title_session_at_turn_start(void) {}

/* PoP: _fail_closed_after_preflight_timeout @ agent/turn_context.py:_fail_closed_after_preflight_timeout */
int _fail_closed_after_preflight_timeout(void) { return 0; }

/* PoP: _review_fork_first_request_pending @ agent/turn_context.py:_review_fork_first_request_pending */
int _review_fork_first_request_pending(void) { return 0; }
