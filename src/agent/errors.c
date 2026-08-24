/*
 * errors.c — canonical C11 port of agent/errors.py.
 *
 * Python's exception classes map to C error-code enumeration + message
 * classification helpers (C has no exceptions; callers propagate codes).
 */

#include <string.h>

/* PoP: errors_code @ agent/errors.py:class SSLConfigurationError
 *                        + class EmptyStreamError
 *                        + class MoAPresetNotFoundError
 *
 * Error taxonomy: SSL config failures, empty provider streams, and missing
 * MoA presets are the three typed failure modes the Python module models. */
enum {
    HS_ERR_NONE = 0,
    HS_ERR_SSL_CONFIGURATION = 1,   /* SSLConfigurationError */
    HS_ERR_EMPTY_STREAM = 2,        /* EmptyStreamError */
    HS_ERR_MOA_PRESET_NOT_FOUND = 3 /* MoAPresetNotFoundError */
};

/* PoP: errors_name @ agent/errors.py (module docstring — typed failure modes) */
const char *errors_name(int code) {
    switch (code) {
    case HS_ERR_SSL_CONFIGURATION:    return "SSLConfigurationError";
    case HS_ERR_EMPTY_STREAM:         return "EmptyStreamError";
    case HS_ERR_MOA_PRESET_NOT_FOUND: return "MoAPresetNotFoundError";
    default:                          return "None";
    }
}

/* PoP: errors_is_empty_stream @ agent/errors.py:class EmptyStreamError */
int errors_is_empty_stream(int code) { return code == HS_ERR_EMPTY_STREAM; }

/* PoP: errors_is_ssl_configuration @ agent/errors.py:class SSLConfigurationError */
int errors_is_ssl_configuration(int code) { return code == HS_ERR_SSL_CONFIGURATION; }
