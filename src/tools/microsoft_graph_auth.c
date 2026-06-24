/*
 * microsoft_graph_auth.c — Name parity wrapper for Python tools/microsoft_graph_auth.py
 *
 * NOTE: The C implementation lives in lib/libmsgraph/ms_graph.c, not here.
 * This file exists ONLY for name parity so that every Python module
 * has a correspondingly-named C file.
 *
 * Port of Python tools/microsoft_graph_auth.py.
 * C implementation: lib/libmsgraph/ms_graph.c
 *
 * Microsoft Graph REST auth (OAuth2 app-only)
 *
 * Key functions ported:
 *   ms_graph_auth_init, ms_graph_token_acquire
 */
#include "hermes.h"
