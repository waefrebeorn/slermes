#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/voice.py: 2 missing functions ── */

/* PoP: start_continuous @ hermes_cli/voice.py:start_continuous */
char *start_continuous(void) { return strdup(""); }

/* PoP: stop_continuous @ hermes_cli/voice.py:stop_continuous */
char *stop_continuous(void) { return strdup(""); }
