#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/models.py: 2 missing functions ── */

char *pricing_cache_scope(void) { return strdup(""); }

char *cached_fetch_api_models(void) { return strdup(""); }
