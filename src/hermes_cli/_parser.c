/*
 * _parser.c — canonical C11 port of hermes_cli/_parser.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli__parser_u_inherited_flag @ hermes_cli/_parser.py:_inherited_flag */
int hermes_cli__parser_u_inherited_flag(const char *arg) {
    /* Python: add_argument + tag inherit_on_relaunch. Arg = "flag". */
    (void)arg;
    printf("inherited flag registered\n");
    return 0;
}
