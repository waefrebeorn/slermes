#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/local_runtime/bootstrap.py: 12 missing functions ── */

/* PoP: _detect_gpu_vendor @ hermes_cli/local_runtime/bootstrap.py:_detect_gpu_vendor */
char *_detect_gpu_vendor(void) { return strdup(""); }

/* PoP: models_dir @ hermes_cli/local_runtime/bootstrap.py:models_dir */
char *models_dir(void) { return strdup(""); }

/* PoP: assets_dir @ hermes_cli/local_runtime/bootstrap.py:assets_dir */
char *assets_dir(void) { return strdup(""); }

/* PoP: staged_models @ hermes_cli/local_runtime/bootstrap.py:staged_models */
char *staged_models(void) { return strdup("[]"); }

/* PoP: staged_model_ids @ hermes_cli/local_runtime/bootstrap.py:staged_model_ids */
char *staged_model_ids(void) { return strdup("[]"); }

/* PoP: _presets_stale @ hermes_cli/local_runtime/bootstrap.py:_presets_stale */
int _presets_stale(void) { return 0; }

/* PoP: _stop_state_server @ hermes_cli/local_runtime/bootstrap.py:_stop_state_server */
void _stop_state_server(void) {}

/* PoP: refresh_local_runtime @ hermes_cli/local_runtime/bootstrap.py:refresh_local_runtime */
void refresh_local_runtime(void) {}

/* PoP: ensure_local_runtime @ hermes_cli/local_runtime/bootstrap.py:ensure_local_runtime */
void ensure_local_runtime(void) {}

/* PoP: shutdown_local_runtime @ hermes_cli/local_runtime/bootstrap.py:shutdown_local_runtime */
void shutdown_local_runtime(void) {}

/* PoP: get_supervisor @ hermes_cli/local_runtime/bootstrap.py:get_supervisor */
char *get_supervisor(void) { return strdup("{}"); }

/* PoP: _start_idle_sweeper @ hermes_cli/local_runtime/bootstrap.py:_start_idle_sweeper */
void _start_idle_sweeper(void) {}
