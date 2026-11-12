#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── cron/lifecycle_guard.py: 6 missing functions ── */




/* PoP: _capped_read_limit @ cron/lifecycle_guard.py:_capped_read_limit */
int _capped_read_limit(void) { return 0; }

/* PoP: lifecycle_scan_root_within_budget @ cron/lifecycle_guard.py:lifecycle_scan_root_within_budget */
char *lifecycle_scan_root_within_budget(void) { return strdup("[]"); }

/* PoP: _budget_exhausted @ cron/lifecycle_guard.py:_budget_exhausted */
int _budget_exhausted(void) { return 0; }
