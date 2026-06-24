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
