#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/local_runtime/capabilities.py: 3 missing functions ── */

char *is_managed_provider(void) { return strdup(""); }


char *managed_model_supports_vision(void) { return strdup(""); }
