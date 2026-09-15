#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/_scan_venv_blockers.py: 8 missing functions ── */

/* PoP: _classify_local_preview_args @ hermes_cli/_scan_venv_blockers.py:_classify_local_preview_args */
char *_classify_local_preview_args(void) { return strdup(""); }

/* PoP: _local_preview_metadata @ hermes_cli/_scan_venv_blockers.py:_local_preview_metadata */
char *_local_preview_metadata(void) { return strdup("{}"); }

/* PoP: _terminate_safe_preview @ hermes_cli/_scan_venv_blockers.py:_terminate_safe_preview */
void _terminate_safe_preview(void) {}

/* PoP: _is_updater_owned_backend @ hermes_cli/_scan_venv_blockers.py:_is_updater_owned_backend */
int _is_updater_owned_backend(void) { return 0; }

/* PoP: _updater_owned_backend_entry @ hermes_cli/_scan_venv_blockers.py:_updater_owned_backend_entry */
char *_updater_owned_backend_entry(void) { return strdup("{}"); }

/* PoP: _deferred_backend_evidence @ hermes_cli/_scan_venv_blockers.py:_deferred_backend_evidence */
char *_deferred_backend_evidence(void) { return strdup(""); }

/* PoP: _spawner_is_this_handoff_desktop @ hermes_cli/_scan_venv_blockers.py:_spawner_is_this_handoff_desktop */
int _spawner_is_this_handoff_desktop(void) { return 0; }

/* PoP: _terminate_safe_main @ hermes_cli/_scan_venv_blockers.py:_terminate_safe_main */
void _terminate_safe_main(void) {}
