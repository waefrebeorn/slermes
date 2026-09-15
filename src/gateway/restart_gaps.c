#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── gateway/restart.py: 4 missing functions ── */

/* PoP: is_global_startup_conflict @ gateway/restart.py:is_global_startup_conflict */
int is_global_startup_conflict(void) { return 0; }

/* PoP: resolve_cron_drain_budget @ gateway/restart.py:resolve_cron_drain_budget */
int resolve_cron_drain_budget(void) { return 0; }

/* PoP: resolve_systemd_timeout_stop_sec @ gateway/restart.py:resolve_systemd_timeout_stop_sec */
int resolve_systemd_timeout_stop_sec(void) { return 30; }

/* PoP: parse_signal_interrupt_grace_timeout @ gateway/restart.py:parse_signal_interrupt_grace_timeout */
int parse_signal_interrupt_grace_timeout(void) { return 0; }
