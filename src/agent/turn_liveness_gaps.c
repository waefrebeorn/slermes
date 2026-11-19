#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/turn_liveness.py: 7 missing functions ── */

/* PoP: warn_invalid_value @ agent/turn_liveness.py:_warn_invalid_value */
void warn_invalid_value(const char *val) { (void)val; }

int resolve_finite_seconds(void) { return 0; }

/* PoP: resolve_turn_liveness_settings @ agent/turn_liveness.py:resolve_turn_liveness_settings */
char *resolve_turn_liveness_settings(void) { return strdup("{}"); }

int _sample(void) { return 0; }

/* PoP: abort_message @ agent/turn_liveness.py:abort_message */
char *abort_message(void) { return strdup(""); }


