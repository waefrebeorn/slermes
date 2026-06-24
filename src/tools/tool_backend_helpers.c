/*
 * tool_backend_helpers.c — Name parity wrapper for Python tools/tool_backend_helpers.py
 *
 * NOTE: The C implementation lives in lib/libtoolbackend/tool_backend.c, not here.
 * This file exists ONLY for name parity so that every Python module
 * has a correspondingly-named C file.
 *
 * Port of Python tools/tool_backend_helpers.py.
 * C implementation: lib/libtoolbackend/tool_backend.c
 *
 * Shared helpers for tool backend selection
 *
 * Key functions ported:
 *   tool_backend_select, tool_backend_available, tool_backend_resolve
 */
#include "hermes.h"
