/*
 * pairing.c — Name parity wrapper for Python gateway/pairing.py
 *
 * NOTE: The C implementation lives in src/gateway/helpers.c, not here.
 * This file exists ONLY for name parity so that every Python module
 * has a correspondingly-named C file.
 *
 * Port of Python gateway/pairing.py, hermes_cli/pairing.py.
 * C implementation: src/gateway/helpers.c
 *
 * Key functions ported:
 *   Gateway pairing/linking logic. C implementation in helpers.c: gw_pairing_generate_code, gw_pairing_verify, gw_pairing_get_status.
 *
 * PoP annotations referencing this module: 1
 */
