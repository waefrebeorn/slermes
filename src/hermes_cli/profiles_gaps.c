#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/profiles.py: 6 missing functions ── */

/* PoP: _seed_model_config @ hermes_cli/profiles.py:_seed_model_config */
void _seed_model_config(void) {}

/* PoP: _served_by_running_multiplexer @ hermes_cli/profiles.py:_served_by_running_multiplexer */
int _served_by_running_multiplexer(void) { return 0; }

/* PoP: _inside_git_checkout @ hermes_cli/profiles.py:_inside_git_checkout */
int _inside_git_checkout(void) { return 0; }

/* PoP: _profile_export_directory @ hermes_cli/profiles.py:_profile_export_directory */
char *_profile_export_directory(void) { return strdup(""); }

/* PoP: get_profile_export_path @ hermes_cli/profiles.py:get_profile_export_path */
char *get_profile_export_path(void) { return strdup(""); }

/* PoP: _scrub_export_secrets @ hermes_cli/profiles.py:_scrub_export_secrets */
char *_scrub_export_secrets(void) { return strdup("{}"); }
