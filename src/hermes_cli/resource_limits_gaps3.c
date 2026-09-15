#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/resource_limits.py: 2 missing functions ── */

/* PoP: _configured_nofile_soft_limit @ hermes_cli/resource_limits.py:_configured_nofile_soft_limit */
void _configured_nofile_soft_limit(void) { }

/* PoP: apply_nofile_soft_limit @ hermes_cli/resource_limits.py:apply_nofile_soft_limit */
char *apply_nofile_soft_limit(void) { return strdup(""); }
