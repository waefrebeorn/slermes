#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/system_prompt.py: 4 missing functions ── */

/* PoP: _plugin_session_info @ agent/system_prompt.py:_plugin_session_info */
char *_plugin_session_info(void) { return strdup(""); }

/* PoP: _frozen_plugin_prompt_sections @ agent/system_prompt.py:_frozen_plugin_prompt_sections */
char *_frozen_plugin_prompt_sections(void) { return strdup("[]"); }

/* PoP: _session_start_like @ agent/system_prompt.py:_session_start_like */
int _session_start_like(void) { return 0; }

/* PoP: _agent_skills_dir @ agent/system_prompt.py:_agent_skills_dir */
char *_agent_skills_dir(void) { return strdup(""); }
