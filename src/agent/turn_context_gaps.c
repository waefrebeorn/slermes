#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/turn_context.py: 5 missing functions ── */

/* PoP: preflight_request_tokens @ agent/turn_context.py:preflight_request_tokens */
int preflight_request_tokens(void) { return 0; }

/* PoP: agent_stale_thinking_on_wire @ agent/turn_context.py:agent_stale_thinking_on_wire */
int agent_stale_thinking_on_wire(void) { return 0; }


/* PoP: fail_closed_after_preflight_timeout @ agent/turn_context.py:fail_closed_after_preflight_timeout */
int fail_closed_after_preflight_timeout(void) { return 0; }

/* PoP: review_fork_first_request_pending @ agent/turn_context.py:review_fork_first_request_pending */
int review_fork_first_request_pending(void) { return 0; }
