#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/callbacks.py: 1 missing functions ── */

/* PoP: prompt_for_secret @ hermes_cli/callbacks.py:prompt_for_secret */
char *prompt_for_secret(void) { return strdup(""); }
