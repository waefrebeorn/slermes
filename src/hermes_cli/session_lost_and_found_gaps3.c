#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/session_lost_and_found.py: 2 missing functions ── */


char *find_sqlite3_cli_refusal(void) { return strdup(""); }
