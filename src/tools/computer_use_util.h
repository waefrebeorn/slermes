/*
 * computer_use_util.h — shared helpers for the computer-use backends.
 *
 * Module-internal header (computer_use.c and the per-platform modules).
 */
#ifndef COMPUTER_USE_UTIL_H
#define COMPUTER_USE_UTIL_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static inline bool _has_cmd(const char *cmd) {
    char buf[256];
    snprintf(buf, sizeof(buf), "which %s >/dev/null 2>&1", cmd);
    return system(buf) == 0;
}

#endif /* COMPUTER_USE_UTIL_H */
