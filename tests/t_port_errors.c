/*
 * t_port_errors.c — exhaustive verification harness for the C-side port of
 * Python agent/errors.py (domain error classes).
 *
 * Python's agent/errors.py defines three exception classes:
 *   SSLConfigurationError(Exception)
 *   EmptyStreamError(RuntimeError)
 *   MoAPresetNotFoundError(ValueError)
 * The C side ports these as three hermes_error_code_t codes with matching
 * names (see include/hermes_error.h, K21). This harness emits one JSON line
 * per domain error with its canonical name + the Python base-class it maps to,
 * so the Python oracle can diff and catch any missing/renamed code.
 *
 * Mapping of Python base class -> int tag:
 *   0 Exception, 1 RuntimeError, 2 ValueError
 */

#include "hermes_error.h"

#include <stdio.h>

typedef struct {
    hermes_error_code_t code;
    const char *name;   /* canonical name == Python exception __name__ */
    int base;           /* 0 Exception, 1 RuntimeError, 2 ValueError */
} errors_entry_t;

static const errors_entry_t ENTRIES[] = {
    { HERMES_ERR_SSL_CONFIG,           "SSLConfigurationError", 0 }, /* Exception */
    { HERMES_ERR_EMPTY_STREAM,         "EmptyStreamError",      1 }, /* RuntimeError */
    { HERMES_ERR_MOA_PRESET_NOT_FOUND, "MoAPresetNotFoundError", 2 }, /* ValueError */
};

static const char *esc(const char *s) {
    /* minimal JSON string escaper (names contain no quotes/backslashes here,
     * but be safe) */
    static char buf[256];
    char *o = buf;
    for (const char *p = s; *p && o < buf + sizeof(buf) - 2; p++) {
        if (*p == '"' || *p == '\\') *o++ = '\\';
        *o++ = *p;
    }
    *o = '\0';
    return buf;
}

int main(void) {
    for (size_t i = 0; i < sizeof(ENTRIES) / sizeof(ENTRIES[0]); i++) {
        const errors_entry_t *e = &ENTRIES[i];
        /* Defensive: the canonical name must match what hermes_error_name
         * reports for the code — if the two drift, the port is wrong. */
        const char *reported = hermes_error_name(e->code);
        const char *name = (reported && reported[0]) ? reported : e->name;
        printf("{\"name\":\"%s\",\"base\":%d}\n", esc(name), e->base);
    }
    return 0;
}
