#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/delegation_output_schema.py: 2 missing functions ── */

/* PoP: coerce_output_schema @ tools/delegation_output_schema.py:coerce_output_schema */
char *coerce_output_schema(void) { return strdup(""); }

/* PoP: validate_output @ tools/delegation_output_schema.py:validate_output */
char *validate_output(void) { return strdup(""); }
