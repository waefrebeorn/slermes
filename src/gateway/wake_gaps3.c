#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── gateway/wake.py: 2 missing functions ── */

/* PoP: _delegation_display_metadata @ gateway/wake.py:_delegation_display_metadata */
void _delegation_display_metadata(void) { }

/* PoP: persist_delegation_delivery @ gateway/wake.py:persist_delegation_delivery */
char *persist_delegation_delivery(void) { return strdup(""); }
