#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/_early_recovery.py: 7 missing functions ── */


int pid_is_running(void) { return 0; }

int marker_owner_is_live(void) { return 0; }

/* PoP: base_interpreter_is_externally_managed @ hermes_cli/_early_recovery.py:base_interpreter_is_externally_managed */
int base_interpreter_is_externally_managed(void) { return 0; }

/* PoP: claim_recovery_lock @ hermes_cli/_early_recovery.py:_claim_recovery_lock */
int claim_recovery_lock(void) { return 1; }


