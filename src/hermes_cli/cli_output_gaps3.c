#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/cli_output.py: 1 missing functions ── */

/* PoP: line_input @ hermes_cli/cli_output.py:line_input */
char *line_input(void) { return strdup(""); }
