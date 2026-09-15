#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/local_runtime/context_policy.py: 5 missing functions ── */

/* PoP: initial_window @ hermes_cli/local_runtime/context_policy.py:initial_window */
int initial_window(void) { return 0; }

/* PoP: growth_decision @ hermes_cli/local_runtime/context_policy.py:growth_decision */
char *growth_decision(void) { return strdup("{}"); }

/* PoP: spill_overrides @ hermes_cli/local_runtime/context_policy.py:spill_overrides */
char *spill_overrides(void) { return strdup("{}"); }

/* PoP: launch_args @ hermes_cli/local_runtime/context_policy.py:launch_args */
char *launch_args(void) { return strdup("[]"); }

/* PoP: ub_logits_bytes @ hermes_cli/local_runtime/context_policy.py:ub_logits_bytes */
int ub_logits_bytes(void) { return 0; }
