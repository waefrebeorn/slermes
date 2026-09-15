#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/providers.py: 2 missing functions ── */

/* PoP: get_label @ hermes_cli/providers.py:get_label */
char *get_label(void) { return strdup(""); }

/* PoP: resolve_provider_full @ hermes_cli/providers.py:resolve_provider_full */
char *resolve_provider_full(void) { return strdup(""); }
