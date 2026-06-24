/*
 * tool_result_classification.c — Name parity wrapper for Python agent/tool_result_classification.py
 *
 * NOTE: The C implementation lives in src/tools/tool_result.c, not here.
 * This file exists ONLY for name parity so that every Python module
 * has a correspondingly-named C file.
 *
 * Port of Python agent/tool_result_classification.py.
 * C implementation: src/tools/tool_result.c
 *
 * Key functions ported:
 *   Tool result classification. C implementation in src/tools/tool_result.c: classify_tool_result, format_tool_result_for_system, is_tool_error, extract_tool_result_summary.
 */
