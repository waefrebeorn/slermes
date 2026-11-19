#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/update_abort_recovery.py: 7 missing functions ── */

/* PoP: serve_unit_recovery_available @ hermes_cli/update_abort_recovery.py:_serve_unit_recovery_available */
int serve_unit_recovery_available(void) { return 0; }

/* PoP: surviving_pre_update_serve_runtimes @ hermes_cli/update_abort_recovery.py:_surviving_pre_update_serve_runtimes */
char *surviving_pre_update_serve_runtimes(void) { return strdup("[]"); }

/* PoP: without_incarnation @ hermes_cli/update_abort_recovery.py:without_incarnation */
char *without_incarnation(void) { return strdup(""); }

/* PoP: qualified_serve_skips @ hermes_cli/update_abort_recovery.py:_qualified_serve_skips */
char *qualified_serve_skips(void) { return strdup("[]"); }



/* PoP: abort_recovery_is_complete @ hermes_cli/update_abort_recovery.py:_abort_recovery_is_complete */
int abort_recovery_is_complete(void) { return 1; }
