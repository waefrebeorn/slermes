#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/title_generator.py: 2 missing functions ── */

/* PoP: apply_instant_title @ agent/title_generator.py:apply_instant_title */
char *apply_instant_title(void) { return strdup(""); }

/* PoP: _session_is_untitled @ agent/title_generator.py:_session_is_untitled */
void _session_is_untitled(void) { }
