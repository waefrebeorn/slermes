/*
 * delivery.c — Name parity wrapper for Python gateway/delivery.py
 *
 * NOTE: The C implementation lives in src/gateway/helpers.c, not here.
 * This file exists ONLY for name parity so that every Python module
 * has a correspondingly-named C file.
 *
 * Port of Python gateway/delivery.py.
 * C implementation: src/gateway/helpers.c
 *
 * Key functions ported:
 *   Message delivery routing. C implementation in helpers.c: gateway_send, gateway_send_message, resolve_platform_target, format_delivery_payload.
 *
 * PoP annotations referencing this module: 13
 */
