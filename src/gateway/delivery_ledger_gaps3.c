#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── gateway/delivery_ledger.py: 2 missing functions ── */

char *release_runtime_claim(void) { return strdup(""); }

char *sweep_failed_for_runtime(void) { return strdup(""); }
