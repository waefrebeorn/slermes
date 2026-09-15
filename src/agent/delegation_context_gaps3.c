#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/delegation_context.py: 2 missing functions ── */

/* PoP: non_dispatcher_owned_context @ agent/delegation_context.py:non_dispatcher_owned_context */
char *non_dispatcher_owned_context(void) { return strdup(""); }

/* PoP: enter_non_dispatcher_owned_context @ agent/delegation_context.py:enter_non_dispatcher_owned_context */
char *enter_non_dispatcher_owned_context(void) { return strdup(""); }
