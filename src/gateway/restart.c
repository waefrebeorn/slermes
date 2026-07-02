/*
 * restart.c — Name parity wrapper for Python gateway/restart.py
 *
 * NOTE: The C implementation lives in src/gateway/helpers.c, not here.
 * This file exists ONLY for name parity so that every Python module
 * has a correspondingly-named C file.
 *
 * Port of Python gateway/restart.py.
 * C implementation: src/gateway/helpers.c
 *
 * Key functions ported:
 *   Gateway restart lifecycle. C implementation in helpers.c: gw_restart_trigger, gw_restart_schedule, gw_restart_cancel.
 *
 * PoP annotations referencing this module: 4
 */
