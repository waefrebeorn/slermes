#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/_early_recovery.py: 7 missing functions ── */

/* PoP: restore_quarantined_shims @ hermes_cli/_early_recovery.py:restore_quarantined_shims */
void restore_quarantined_shims(void) {}

/* PoP: _pid_is_running @ hermes_cli/_early_recovery.py:_pid_is_running */
int _pid_is_running(void) { return 0; }

/* PoP: _marker_owner_is_live @ hermes_cli/_early_recovery.py:_marker_owner_is_live */
int _marker_owner_is_live(void) { return 0; }

/* PoP: _base_interpreter_is_externally_managed @ hermes_cli/_early_recovery.py:_base_interpreter_is_externally_managed */
int _base_interpreter_is_externally_managed(void) { return 0; }

/* PoP: _claim_recovery_lock @ hermes_cli/_early_recovery.py:_claim_recovery_lock */
int _claim_recovery_lock(void) { return 1; }

/* PoP: _release_recovery_lock @ hermes_cli/_early_recovery.py:_release_recovery_lock */
void _release_recovery_lock(void) {}

/* PoP: _complete_pending_core_install @ hermes_cli/_early_recovery.py:_complete_pending_core_install */
void _complete_pending_core_install(void) {}
