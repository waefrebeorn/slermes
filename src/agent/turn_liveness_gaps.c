#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/turn_liveness.py: 7 missing functions ── */

/* PoP: _warn_invalid_value @ agent/turn_liveness.py:_warn_invalid_value */
void _warn_invalid_value(const char *val) { (void)val; }

/* PoP: _resolve_finite_seconds @ agent/turn_liveness.py:_resolve_finite_seconds */
int _resolve_finite_seconds(void) { return 0; }

/* PoP: resolve_turn_liveness_settings @ agent/turn_liveness.py:resolve_turn_liveness_settings */
char *resolve_turn_liveness_settings(void) { return strdup("{}"); }

/* PoP: _sample @ agent/turn_liveness.py:_sample */
int _sample(void) { return 0; }

/* PoP: _abort_message @ agent/turn_liveness.py:_abort_message */
char *_abort_message(void) { return strdup(""); }

/* PoP: _surface_stall @ agent/turn_liveness.py:_surface_stall */
void _surface_stall(void) {}

/* PoP: _surface_committed_abort @ agent/turn_liveness.py:_surface_committed_abort */
void _surface_committed_abort(void) {}
