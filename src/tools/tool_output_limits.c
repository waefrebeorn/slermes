/*
 * tool_output_limits.c — Name parity wrapper for Python tools/tool_output_limits.py
 *
 * NOTE: The C implementation lives in lib/libtooloutput/tool_output.c, not here.
 * This file exists ONLY for name parity so that every Python module
 * has a correspondingly-named C file.
 *
 * Port of Python tools/tool_output_limits.py.
 * C implementation: lib/libtooloutput/tool_output.c
 *
 * Configurable tool-output truncation limits
 *
 * Key functions ported:
 *   tool_output_limit_get, tool_output_truncate
 */
#include "hermes_core_types.h"
