/* Slermes C11 port — hermes_cli/win_pty_bridge.py */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: is_available_port @ hermes_cli/win_pty_bridge.py:is_available */
bool is_available_port(json_t * cls) {
    return false;
}

/* PoP: pid_port @ hermes_cli/win_pty_bridge.py:pid */
int pid_port(void) {
    return 0;
}

/* PoP: is_alive_port @ hermes_cli/win_pty_bridge.py:is_alive */
bool is_alive_port(void) {
    return false;
}

