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

/* PoP: _is_install_tree @ agent/runtime_cwd.py:_is_install_tree */
void agent_runtime_cwd_is_install_tree(void) { }

/* PoP: scoped_session_cwd @ agent/runtime_cwd.py:scoped_session_cwd */
void agent_runtime_cwd_scoped_session_cwd(void) { }

/* PoP: reset_session_cwd @ agent/runtime_cwd.py:reset_session_cwd */
void agent_runtime_cwd_reset_session_cwd(void) { }

/* PoP: scope_terminal_cwd @ agent/runtime_cwd.py:scope_terminal_cwd */
void agent_runtime_cwd_scope_terminal_cwd(void) { }

/* PoP: _existing_dir @ agent/runtime_cwd.py:_existing_dir */
void agent_runtime_cwd_existing_dir(void) { }

/* PoP: _resolve_configured_cwd @ agent/runtime_cwd.py:_resolve_configured_cwd */
void agent_runtime_cwd_resolve_configured_cwd(void) { }
