/**
 * @file hermes_gateway_pairing.h
 * @brief Pairing secure write API (port of Python gateway/pairing.py).
 */
#ifndef HERMES_GATEWAY_PAIRING_H
#define HERMES_GATEWAY_PAIRING_H

#include "hermes_gateway_types.h"

/* ================================================================
 *  Pairing — Secure Write
 * ================================================================ */

/* Write data to file atomically with owner-only permissions (0600).
 * Uses temp file + mkstemp + rename. Returns true on success. */
/* PoP: secure_write @ gateway/pairing.py:_secure_write */
bool secure_write(const char *path, const char *data);

#endif /* HERMES_GATEWAY_PAIRING_H */