#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/_subprocess_compat.py: 6 missing functions ── */

/* PoP: harden_git_argv @ hermes_cli/_subprocess_compat.py:harden_git_argv */
char *harden_git_argv(void) { return strdup("[]"); }

/* PoP: resolve_node_command @ hermes_cli/_subprocess_compat.py:resolve_node_command */
char *resolve_node_command(void) { return strdup(""); }

/* PoP: _text_names_hermes @ hermes_cli/_subprocess_compat.py:_text_names_hermes */
int _text_names_hermes(void) { return 0; }

/* PoP: _process_command_is_hermes @ hermes_cli/_subprocess_compat.py:_process_command_is_hermes */
int _process_command_is_hermes(void) { return 0; }

/* PoP: pid_is_hermes @ hermes_cli/_subprocess_compat.py:pid_is_hermes */
int pid_is_hermes(void) { return 0; }

/* PoP: bounded_probe_run @ hermes_cli/_subprocess_compat.py:bounded_probe_run */
char *bounded_probe_run(void) { return strdup("{}"); }
