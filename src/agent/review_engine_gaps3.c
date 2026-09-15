#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/review_engine.py: 2 missing functions ── */

/* PoP: _load_review_credentials_cfg @ agent/review_engine.py:_load_review_credentials_cfg */
void _load_review_credentials_cfg(void) { }

/* PoP: start_review @ agent/review_engine.py:start_review */
char *start_review(void) { return strdup(""); }
