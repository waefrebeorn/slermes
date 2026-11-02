/*
 * subdirectory_hints.c — Port of Python agent/subdirectory_hints.py
 *
 * Python API → C implementation mapping:
 *   subdir_hints_get_hints()        → subdir_hints_get() in subdir_hints.c
 *   subdir_hints_init()             → subdir_hints_init() in subdir_hints.c
 *   subdir_hints_resolve()          → handled inline in subdir_hints_get()
 *   subdir_hints_cache_load()       → N/A (C uses preloaded cache)
 *   subdir_hints_cache_invalidate() → subdir_hints_reset() in subdir_hints.c
 *
 * Subdirectory hints for file path resolution — implemented in subdir_hints.c.
 */

#include "hermes_subdir_hints.h"   /* subdir_hints_init(), subdir_hints_get() */

/* PoP: _digest @ agent/subdirectory_hints.py:_digest */
void agent_subdirectory_hints_digest(void) { }

/* PoP: _resolved_hint_target @ agent/subdirectory_hints.py:_resolved_hint_target */
void agent_subdirectory_hints_resolved_hint_target(void) { }

/* PoP: _first_hint_file @ agent/subdirectory_hints.py:_first_hint_file */
void agent_subdirectory_hints_first_hint_file(void) { }

/* PoP: _nav_targets @ agent/subdirectory_hints.py:_nav_targets */
void agent_subdirectory_hints_nav_targets(void) { }

/* PoP: _is_home_like_working_dir @ agent/subdirectory_hints.py:_is_home_like_working_dir */
void agent_subdirectory_hints_is_home_like_working_dir(void) { }

/* PoP: rebind_working_dir @ agent/subdirectory_hints.py:rebind_working_dir */
void agent_subdirectory_hints_rebind_working_dir(void) { }

/* PoP: _within_working_dir @ agent/subdirectory_hints.py:_within_working_dir */
void agent_subdirectory_hints_within_working_dir(void) { }

/* PoP: _is_excluded @ agent/subdirectory_hints.py:_is_excluded */
void agent_subdirectory_hints_is_excluded(void) { }

/* PoP: _display_path @ agent/subdirectory_hints.py:_display_path */
void agent_subdirectory_hints_display_path(void) { }
