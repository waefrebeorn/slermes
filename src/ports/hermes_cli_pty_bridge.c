/* Slermes C11 port — hermes_cli/pty_bridge.py */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: is_available @ hermes_cli/pty_bridge.py:is_available */
bool is_available(json_t * cls) {
    return false;
}

/* PoP: pid @ hermes_cli/pty_bridge.py:pid */
int pid(void) {
    return 0;
}

/* PoP: is_alive @ hermes_cli/pty_bridge.py:is_alive */
bool is_alive(void) {
    return false;
}

