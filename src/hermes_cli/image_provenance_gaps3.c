#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/image_provenance.py: 2 missing functions ── */

/* PoP: _invalid @ hermes_cli/image_provenance.py:_invalid */
void _invalid(void) { }

/* PoP: read_image_provenance @ hermes_cli/image_provenance.py:read_image_provenance */
char *read_image_provenance(void) { return strdup(""); }
