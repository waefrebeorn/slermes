#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/fast_mode.py: 1 missing functions ── */

/* PoP: effective_request_overrides @ agent/fast_mode.py:effective_request_overrides */
char *effective_request_overrides(void) { return strdup(""); }
