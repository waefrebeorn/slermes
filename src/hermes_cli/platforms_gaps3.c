#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/platforms.py: 1 missing functions ── */

/* PoP: get_all_platforms @ hermes_cli/platforms.py:get_all_platforms */
char *get_all_platforms(void) { return strdup(""); }
