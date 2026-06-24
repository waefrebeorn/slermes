/*
 * managed_tool_gateway.c — Name parity wrapper for Python tools/managed_tool_gateway.py
 *
 * NOTE: The C implementation lives in lib/libmangateway/managed_gateway.c, not here.
 * This file exists ONLY for name parity so that every Python module
 * has a correspondingly-named C file.
 *
 * Port of Python tools/managed_tool_gateway.py.
 * C implementation: lib/libmangateway/managed_gateway.c
 *
 * Managed-tool gateway helpers for Nous-hosted vendor passthroughs
 *
 * Key functions ported:
 *   gateway dispatch, tool backend selection helpers
 */
#include "hermes.h"
