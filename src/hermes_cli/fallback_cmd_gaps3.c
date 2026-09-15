#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/fallback_cmd.py: 1 missing functions ── */

/* PoP: cmd_fallback @ hermes_cli/fallback_cmd.py:cmd_fallback */
char *cmd_fallback(void) { return strdup(""); }
