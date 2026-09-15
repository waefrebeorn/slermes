#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/environments/local.py: 12 missing functions ── */

/* PoP: _default_terminal_temp_dir @ tools/environments/local.py:_default_terminal_temp_dir */
char *_default_terminal_temp_dir(void) { return strdup("/tmp"); }

/* PoP: cleanup_terminal_temp_cache @ tools/environments/local.py:cleanup_terminal_temp_cache */
void cleanup_terminal_temp_cache(void) {}

/* PoP: _prune_terminal_temp_once @ tools/environments/local.py:_prune_terminal_temp_once */
void _prune_terminal_temp_once(void) {}

/* PoP: _matches_terminal_first_party_prefix @ tools/environments/local.py:_matches_terminal_first_party_prefix */
int _matches_terminal_first_party_prefix(void) { return 0; }

/* PoP: _buzz_terminal_context_active @ tools/environments/local.py:_buzz_terminal_context_active */
int _buzz_terminal_context_active(void) { return 0; }

/* PoP: _is_terminal_first_party_env @ tools/environments/local.py:_is_terminal_first_party_env */
int _is_terminal_first_party_env(void) { return 0; }

/* PoP: _plugin_terminal_env_strip_keys @ tools/environments/local.py:_plugin_terminal_env_strip_keys */
char *_plugin_terminal_env_strip_keys(void) { return strdup("[]"); }

/* PoP: _build_hermes_repo_root_aliases @ tools/environments/local.py:_build_hermes_repo_root_aliases */
char *_build_hermes_repo_root_aliases(void) { return strdup("[]"); }

/* PoP: _validated_runtime_venv @ tools/environments/local.py:_validated_runtime_venv */
char *_validated_runtime_venv(void) { return strdup(""); }

/* PoP: _get_hermes_site_packages @ tools/environments/local.py:_get_hermes_site_packages */
char *_get_hermes_site_packages(void) { return strdup(""); }

/* PoP: _strip_hermes_owned_pythonpath_and_runtime_markers @ tools/environments/local.py:_strip_hermes_owned_pythonpath_and_runtime_markers */
void _strip_hermes_owned_pythonpath_and_runtime_markers(void) {}

/* PoP: _strip_hermes_owned_pythonpath @ tools/environments/local.py:_strip_hermes_owned_pythonpath */
void _strip_hermes_owned_pythonpath(void) {}
