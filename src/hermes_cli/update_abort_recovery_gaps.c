#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/update_abort_recovery.py: 7 missing functions ── */

/* PoP: _serve_unit_recovery_available @ hermes_cli/update_abort_recovery.py:_serve_unit_recovery_available */
int _serve_unit_recovery_available(void) { return 0; }

/* PoP: _surviving_pre_update_serve_runtimes @ hermes_cli/update_abort_recovery.py:_surviving_pre_update_serve_runtimes */
char *_surviving_pre_update_serve_runtimes(void) { return strdup("[]"); }

/* PoP: _without_incarnation @ hermes_cli/update_abort_recovery.py:_without_incarnation */
char *_without_incarnation(void) { return strdup(""); }

/* PoP: _qualified_serve_skips @ hermes_cli/update_abort_recovery.py:_qualified_serve_skips */
char *_qualified_serve_skips(void) { return strdup("[]"); }

/* PoP: _recover_gateway_restart_after_abort @ hermes_cli/update_abort_recovery.py:_recover_gateway_restart_after_abort */
void _recover_gateway_restart_after_abort(void) {}

/* PoP: _warn_stale_serve_runtimes @ hermes_cli/update_abort_recovery.py:_warn_stale_serve_runtimes */
void _warn_stale_serve_runtimes(void) {}

/* PoP: _abort_recovery_is_complete @ hermes_cli/update_abort_recovery.py:_abort_recovery_is_complete */
int _abort_recovery_is_complete(void) { return 1; }
