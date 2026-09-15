#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/redact.py: 1 missing functions ── */

/* PoP: register_redaction_patterns @ agent/redact.py:register_redaction_patterns */
char *register_redaction_patterns(void) { return strdup(""); }
