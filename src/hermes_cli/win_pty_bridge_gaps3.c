#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/win_pty_bridge.py: 1 missing functions ── */

/* PoP: pid @ hermes_cli/win_pty_bridge.py:pid (renamed to avoid collision with pty_bridge.pid) */
char *win_pty_bridge_pid(void) { return strdup(""); }
