#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/environments/local.py: 12 missing functions ── */

/* PoP: default_terminal_temp_dir @ tools/environments/local.py:default_terminal_temp_dir */
char *default_terminal_temp_dir(void) { return strdup("/tmp"); }



/* PoP: matches_terminal_first_party_prefix @ tools/environments/local_env_policy.py:matches_terminal_first_party_prefix */
int matches_terminal_first_party_prefix(void) { return 0; }

/* PoP: buzz_terminal_context_active @ tools/environments/local_env_policy.py:buzz_terminal_context_active */
int buzz_terminal_context_active(void) { return 0; }

/* PoP: is_terminal_first_party_env @ tools/environments/local_env_policy.py:is_terminal_first_party_env */
int is_terminal_first_party_env(void) { return 0; }

/* PoP: plugin_terminal_env_strip_keys @ tools/environments/local_env_policy.py:plugin_terminal_env_strip_keys */
char *plugin_terminal_env_strip_keys(void) { return strdup("[]"); }

/* PoP: build_hermes_repo_root_aliases @ tools/environments/local_pythonpath.py:build_hermes_repo_root_aliases */
char *build_hermes_repo_root_aliases(void) { return strdup("[]"); }

/* PoP: validated_runtime_venv @ tools/environments/local_pythonpath.py:validated_runtime_venv */
char *validated_runtime_venv(void) { return strdup(""); }

/* PoP: get_hermes_site_packages @ tools/environments/local_pythonpath.py:get_hermes_site_packages */
char *get_hermes_site_packages(void) { return strdup(""); }


