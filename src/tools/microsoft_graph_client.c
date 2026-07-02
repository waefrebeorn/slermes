/*
 * microsoft_graph_client.c — Name parity wrapper for Python tools/microsoft_graph_client.py
 *
 * NOTE: The C implementation lives in lib/libmsgraph/ms_graph.c, not here.
 * This file exists ONLY for name parity so that every Python module
 * has a correspondingly-named C file.
 *
 * Port of Python tools/microsoft_graph_client.py.
 * C implementation: lib/libmsgraph/ms_graph.c
 *
 * Microsoft Graph REST client
 *
 * Key functions ported:
 *   ms_graph_request, ms_graph_get_user, ms_graph_send_mail
 */
#include "hermes.h"
