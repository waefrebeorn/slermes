/*
 * runtime_cwd.c — Port of Python agent/runtime_cwd.py
 *
 * Python API → C implementation mapping:
 *   get_runtime_cwd()       → resolve_agent_cwd() in prompt_builder.c (hermes_system_prompt.h)
 *   set_runtime_cwd()       → N/A (CWD is resolved per-turn, not stored)
 *   format_cwd_in_prompt()  → build_environment_hints() in prompt_builder.c
 *
 * Runtime working directory resolution — ported inside prompt_builder.c
 * as part of the environment hints / context files assembly.
 */

#include "hermes_system_prompt.h"   /* resolve_agent_cwd(), build_environment_hints() */
