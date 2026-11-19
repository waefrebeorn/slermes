#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/model_catalog.py: 3 missing functions ── */

char *refresh_interval_seconds(void) { return strdup(""); }

char *refresh_catalogs(void) { return strdup(""); }

char *seed_cache_from_checkout(void) { return strdup(""); }
