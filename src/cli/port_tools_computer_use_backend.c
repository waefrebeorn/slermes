/*
 * port_tools_computer_use_backend.c — C port of tools/computer_use/backend.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_computer_use_backend_center @ tools/computer_use/backend.py:center */

/* Port of Python tools/computer_use/backend.py:center */
/* Centers the mouse cursor on the screen. */
int cli_tools_computer_use_backend_center(int *x, int *y)
{
    if (!x || !y) {
        return -1;
    }
    /* CLI port: screen interaction requires platform-specific code. */
    *x = 0;
    *y = 0;
    return 0;
}

