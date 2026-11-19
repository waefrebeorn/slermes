#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/relay_llm.py: 5 missing functions ── */

/* PoP: relay_protocol @ agent/relay_llm.py:relay_protocol */
char *relay_protocol(void) { return strdup(""); }

char *relay_operation_name(void) { return strdup(""); }

char *_relay_metadata(void) { return strdup("{}"); }


