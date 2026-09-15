#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/_parser.py: 2 missing functions ── */

/* PoP: top_level_value_flag_sets @ hermes_cli/_parser.py:top_level_value_flag_sets */
char *top_level_value_flag_sets(void) { return strdup(""); }

/* PoP: build_top_level_parser @ hermes_cli/_parser.py:build_top_level_parser */
char *build_top_level_parser(void) { return strdup(""); }
