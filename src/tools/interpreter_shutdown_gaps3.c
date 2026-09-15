#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/interpreter_shutdown.py: 1 missing functions ── */

/* PoP: interpreter_shutting_down @ tools/interpreter_shutdown.py:interpreter_shutting_down */
char *interpreter_shutting_down(void) { return strdup(""); }
