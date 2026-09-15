#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/gitlock.py: 1 missing functions ── */

/* PoP: clear_stale_tmp_packs @ hermes_cli/gitlock.py:clear_stale_tmp_packs */
char *clear_stale_tmp_packs(void) { return strdup(""); }
