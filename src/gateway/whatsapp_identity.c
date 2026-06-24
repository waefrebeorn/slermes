/*
 * whatsapp_identity.c — Name parity wrapper for Python gateway/whatsapp_identity.py
 *
 * NOTE: The C implementation lives in src/gateway/helpers.c, not here.
 * This file exists ONLY for name parity so that every Python module
 * has a correspondingly-named C file.
 *
 * Port of Python gateway/whatsapp_identity.py.
 * C implementation: src/gateway/helpers.c
 *
 * Key functions ported:
 *   WhatsApp identity verification. C implementation in helpers.c: gw_whatsapp_verify_token, gw_whatsapp_identity_check.
 *
 * PoP annotations referencing this module: 5
 */
