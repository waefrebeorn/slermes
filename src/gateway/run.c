/*
 * run.c — Name parity wrapper for Python gateway/run.py
 *
 * NOTE: The C implementation lives in src/gateway/server.c, not here.
 * This file exists ONLY for name parity so that every Python module
 * has a correspondingly-named C file.
 *
 * Port of Python gateway/run.py.
 * C implementation: src/gateway/server.c
 *
 * Key functions ported:
 *   Main gateway runner. C implementation in server.c: gateway_run, gateway_start, gateway_stop, gateway_loop.
 *
 * PoP annotations referencing this module: 64
 */
