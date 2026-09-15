#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/pty_bridge.py: 1 missing functions ── */

/* PoP: pid @ hermes_cli/pty_bridge.py:pid */
char *pid(void) { return strdup(""); }
