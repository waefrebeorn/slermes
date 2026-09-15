#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/desktop_ui.py: 1 missing functions ── */

/* PoP: user_enabled @ tools/desktop_ui.py:user_enabled */
char *user_enabled(void) { return strdup(""); }
